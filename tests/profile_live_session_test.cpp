// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "profile_live_session.hpp"

#include <iostream>

namespace {

bool expect(bool condition, const char *message)
{
	if (condition)
		return true;
	std::cerr << message << '\n';
	return false;
}

} // namespace

int main()
{
	Profile profile;
	profile.frame_signing_uses_main_output = true;
	profile.frame_signing_output_name = QStringLiteral("Aitum TikTok");

	const PreparedLive live{
		.session_id = QStringLiteral("session-1"),
		.stream_id = QStringLiteral("stream-1"),
		.room_id = QStringLiteral("room-1"),
		.server = QStringLiteral("rtmp://example.invalid/live"),
		.key = QStringLiteral("stream-key"),
		.dual_server = QStringLiteral("rtmp://example.invalid/landscape"),
		.dual_key = QStringLiteral("landscape-key"),
		.tiktok_username = QStringLiteral("live-session-user"),
	};

	ProfileLiveSession::reserve(profile, live);
	if (!expect(profile.live && !profile.preparing && !profile.ending &&
		!profile.recovering && !profile.session_uncertain,
		"A reserved session must become the sole active, certain session.") ||
		!expect(profile.live_id == live.session_id && profile.stream_id == live.stream_id &&
			profile.stream_server == live.server && profile.stream_key == live.key &&
			profile.dual_stream_server == live.dual_server && profile.dual_stream_key == live.dual_key &&
			profile.live_tiktok_username == live.tiktok_username,
			"A reserved session must preserve every primary and Dual Layout credential."))
		return 1;

	ProfileLiveSession::mark_uncertain(profile, live);
	if (!expect(profile.live && profile.session_uncertain,
		"An unsuccessful cleanup must retain the reservation as uncertain."))
		return 2;

	ProfileLiveSession::clear(profile);
	if (!expect(!profile.live && !profile.preparing && !profile.ending &&
		!profile.recovering && !profile.session_uncertain && profile.live_id.isEmpty() &&
		profile.stream_id.isEmpty() && profile.stream_server.isEmpty() && profile.stream_key.isEmpty() &&
		profile.dual_stream_server.isEmpty() && profile.dual_stream_key.isEmpty() &&
		profile.live_tiktok_username.isEmpty() &&
		!profile.frame_signing_uses_main_output && profile.frame_signing_output_name.isEmpty(),
		"Clearing a session must remove all volatile session and signing attachment state."))
		return 3;

	profile.output_name = QStringLiteral("TikTok Vertical");
	profile.dual_layout_enabled = true;
	profile.dual_output_name = QStringLiteral("TikTok Landscape");
	profile.frame_signing_uses_main_output = true;
	profile.frame_signing_output_name = QStringLiteral("TikTok Vertical");
	ProfileLiveSession::clear_output_assignment(profile);
	if (!expect(profile.output_name.isEmpty() && !profile.dual_layout_enabled && profile.dual_output_name.isEmpty() &&
		!profile.frame_signing_uses_main_output &&
		profile.frame_signing_output_name.isEmpty(),
		"Returning to setup must clear every paired output and signing attachment."))
		return 4;

	std::cout << "Profile live session transitions passed.\n";
	return 0;
}
