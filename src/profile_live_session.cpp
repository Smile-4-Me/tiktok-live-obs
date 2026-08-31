// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "profile_live_session.hpp"

namespace ProfileLiveSession {

void reserve(Profile &profile, const PreparedLive &live)
{
	reserve(profile, live.session_id, live.stream_id, live.server, live.key);
	profile.dual_stream_server = live.dual_server;
	profile.dual_stream_key = live.dual_key;
	profile.live_tiktok_username = live.tiktok_username.trimmed();
}

void reserve(Profile &profile, const QString &session_id, const QString &stream_id,
	const QString &server, const QString &key)
{
	profile.live = true;
	profile.preparing = false;
	profile.ending = false;
	profile.recovering = false;
	profile.session_uncertain = false;
	profile.live_id = session_id;
	profile.stream_id = stream_id;
	profile.stream_server = server;
	profile.stream_key = key;
}

void mark_uncertain(Profile &profile, const PreparedLive &live)
{
	reserve(profile, live);
	profile.session_uncertain = true;
}

void clear(Profile &profile)
{
	profile.frame_signing_uses_main_output = false;
	profile.frame_signing_output_name.clear();
	profile.live = false;
	profile.preparing = false;
	profile.ending = false;
	profile.recovering = false;
	profile.session_uncertain = false;
	profile.live_id.clear();
	profile.stream_id.clear();
	profile.stream_server.clear();
	profile.stream_key.clear();
	profile.dual_stream_server.clear();
	profile.dual_stream_key.clear();
	profile.live_tiktok_username.clear();
}

void clear_output_assignment(Profile &profile)
{
	profile.output_name.clear();
	profile.dual_layout_enabled = false;
	profile.dual_output_name.clear();
	profile.frame_signing_uses_main_output = false;
	profile.frame_signing_output_name.clear();
}

} // namespace ProfileLiveSession
