// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "tiktok_studio_account.hpp"
#include "tiktok_studio_game_tags.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <mutex>
#include <QVector>

struct TikTokStudioQrCode {
	QByteArray png;
	TikTokStudioAccountCredentials account;
};

enum class TikTokStudioQrState { Waiting, Scanned, Confirmed, Expired };

struct TikTokStudioQrPoll {
	TikTokStudioQrState state = TikTokStudioQrState::Waiting;
	TikTokStudioAccountCredentials account;
	bool can_go_live = false;
	QString application_status;
};

struct TikTokStudioAccountInfo {
	TikTokStudioAccountCredentials account;
	bool can_go_live = false;
	QString application_status;
};

struct TikTokStudioLive {
	TikTokStudioAccountCredentials account;
	QString room_id;
	QString stream_id;
	QString owner_user_id;
	QString server;
	QString key;
};

struct TikTokStudioEndResult {
	TikTokStudioAccountCredentials account;
	bool ended = false;
	bool stale_session = false;
	QString error;
};

struct TikTokStudioHeartbeatResult {
	TikTokStudioAccountCredentials account;
	bool room_is_living = false;
	QString error;
};

class TikTokStudioClient final : public QObject {
public:
	using QrCallback = std::function<void(TikTokStudioQrCode code, QString error)>;
	using QrPollCallback = std::function<void(TikTokStudioQrPoll poll, QString error)>;
	using AccountCallback = std::function<void(TikTokStudioAccountInfo info, QString error)>;
	using GameTagsCallback = std::function<void(QVector<TikTokStudioGameTag> tags,
		TikTokStudioAccountCredentials account, QString error)>;
	using LiveCallback = std::function<void(TikTokStudioLive live, QString error)>;
	using EndCallback = std::function<void(TikTokStudioEndResult result)>;
	using HeartbeatCallback = std::function<void(TikTokStudioHeartbeatResult result)>;

	explicit TikTokStudioClient(QObject *parent = nullptr);
	~TikTokStudioClient() override;

	void begin_qr_login(TikTokStudioAccountCredentials account, QrCallback completion);
	void poll_qr_login(QrPollCallback completion);
	void cancel_qr_login();
	void verify_account(TikTokStudioAccountCredentials account, AccountCallback completion);
	void fetch_game_tags(TikTokStudioAccountCredentials account, GameTagsCallback completion);
	// Inspecting is read-only: it obtains the current reusable room and refreshed
	// cookies without sending the prepare heartbeat or starting an OBS output.
	void find_continuable_live(TikTokStudioAccountCredentials account, LiveCallback completion);
	void resume_live(TikTokStudioAccountCredentials account, LiveCallback completion);
	void start_live(TikTokStudioAccountCredentials account, const QString &title,
		const QString &hashtag_id, const QString &game_tag_id, bool mature, LiveCallback completion);
	void heartbeat(TikTokStudioAccountCredentials account, const QString &room_id,
		const QString &stream_id, int status, HeartbeatCallback completion);
	void end_live(TikTokStudioAccountCredentials account, const QString &room_id,
		const QString &stream_id, EndCallback completion);

private:
	struct LoginState;
	struct PendingGameTagsCallback {
		TikTokStudioAccountCredentials account;
		GameTagsCallback completion;
	};
	std::mutex login_mutex_;
	std::shared_ptr<LoginState> login_;
	quint64 login_generation_ = 0;
	std::mutex game_tags_mutex_;
	QVector<TikTokStudioGameTag> game_tags_cache_;
	QVector<PendingGameTagsCallback> game_tags_callbacks_;
	bool game_tags_loading_ = false;
	bool shutting_down_ = false;
};
