// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "frame_signing.hpp"
#include "sei_metadata.hpp"

#include <QHash>
#include <QObject>

#include <obs-frontend-api.h>

#include <functional>
#include <future>
#include <memory>
#include <vector>

struct obs_output;
struct encoder_packet;
struct encoder_packet_time;

class QTimer;

class OutputSigningManager final : public QObject {
public:
	using Completion = std::function<void(bool attached, QString error)>;
	using QuotaCallback = std::function<void(const RapidApiQuota &quota)>;

	explicit OutputSigningManager(QObject *parent = nullptr);
	~OutputSigningManager() override;

	// Fetches a five-minute RapidAPI batch off the UI/encoder threads, then
	// attaches the OBS packet callback before the selected output starts. An
	// empty output name selects OBS' main streaming output.
	void prepare_and_attach(const QString &output_name, HostedSigningServiceConfig api,
		SignedSeiConfig signing, Completion completion, QuotaCallback quota_callback = {});
	void detach(const QString &output_name);
	void detach_all();
	[[nodiscard]] bool attached(const QString &output_name) const;

private:
	struct Session;

	static void packet_callback(obs_output *output, encoder_packet *packet,
		encoder_packet_time *packet_time, void *param);
	static void frontend_event_callback(obs_frontend_event event, void *param);
	void finish_initial_prepare(const std::shared_ptr<Session> &session, FrameSignBatch batch,
		Completion completion);
	void finish_fatal(const std::shared_ptr<Session> &session, const QString &error);
	void rebind_main_output();
	void refresh_due_sessions();
	bool launch_worker(std::function<void()> work);
	void reap_workers();

	QHash<QString, std::shared_ptr<Session>> sessions_;
	QHash<QString, std::shared_ptr<Session>> pending_;
	QHash<QString, FrameSignBatch> shared_batches_;
	QTimer *refresh_timer_ = nullptr;
	std::vector<std::future<void>> workers_;
	bool shutting_down_ = false;
};
