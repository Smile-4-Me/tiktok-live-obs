// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QHash>
#include <QString>

#include <memory>

class NativeOutputManager final {
public:
	struct CreateResult {
		QString output_name;
		QString error;
	};

	NativeOutputManager() = default;
	~NativeOutputManager();

	NativeOutputManager(const NativeOutputManager &) = delete;
	NativeOutputManager &operator=(const NativeOutputManager &) = delete;

	CreateResult create(const QString &profile_id, const QString &server, const QString &key);
	bool start(const QString &profile_id, QString *error = nullptr);
	void remove(const QString &profile_id);
	void remove_all();
	[[nodiscard]] bool contains(const QString &profile_id) const;
	[[nodiscard]] bool active(const QString &profile_id) const;
	[[nodiscard]] QString last_error(const QString &profile_id) const;
	[[nodiscard]] QString output_name(const QString &profile_id) const;
	[[nodiscard]] QString configured_video_codec() const;

private:
	struct Session;
	QHash<QString, std::shared_ptr<Session>> sessions_;
};
