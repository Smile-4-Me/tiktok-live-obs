// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QString>

// The dock works with these provider-neutral values after an account has been
// connected. Provider adapters translate their own APIs into this contract.
// Keeping it free of Streamlabs, TikTok Studio, and Aitum types makes it safe
// to reuse from the session coordinator and from future providers.

enum class ProviderAuthenticationKind {
	ManualCredentials,
	BrowserToken,
	QrCode,
};

enum class ProviderSessionKind {
	// The user supplies a durable server/key pair. No remote LIVE exists for
	// the plugin to create, recover, or end.
	LocalCredentials,
	// The provider creates and owns a remote LIVE session.
	RemoteSession,
};

// Media signing is independent from a provider's authentication and LIVE
// session APIs. This policy tells the shared dock and output pipeline whether
// signing controls are unavailable, user-selectable, or mandatory.
enum class FrameSigningRequirement {
	NotSupported,
	Optional,
	Required,
};

struct ProviderCapabilities {
	ProviderAuthenticationKind authentication = ProviderAuthenticationKind::BrowserToken;
	ProviderSessionKind session = ProviderSessionKind::RemoteSession;

	bool supports_account_refresh = true;
	bool supports_categories = false;
	bool supports_topics = false;
	bool supports_session_recovery = false;
	// When enabled, the shared stream-target selector offers a target that only
	// prepares and displays a newly generated URL/key. Providers based on an
	// already supplied, one-time URL/key do not need that target.
	bool supports_main_obs_output = true;

	// This describes a provider requirement, not an implementation choice. The
	// media pipeline decides whether and how it can satisfy the requirement.
	FrameSigningRequirement frame_signing = FrameSigningRequirement::NotSupported;
};

struct ProviderAccountStatus {
	QString username;
	QString status;
	bool can_go_live = false;
	bool live_access_is_confirmed = false;
};

// Provider-neutral option used by providers that offer a selectable LIVE
// topic, game, or category catalogue. The dock never receives vendor response
// types.
struct ProviderCatalogEntry {
	QString id;
	QString name;
};

// Stable references supplied by the dock to a provider adapter. The adapter
// may use profile_id to retrieve its own secret and account_id to retrieve an
// account-scoped secret. Neither identifier is a TikTok credential.
struct ProviderAccountReference {
	QString profile_id;
	QString account_id;
};

struct LiveRequest {
	QString title;
	QString topic_id;
	QString category_id;
	bool mature = false;
};

// A provider hands the same credential pair to every output path. Aitum and
// manual display never need to know which provider produced it.
struct PreparedLive {
	QString session_id;
	QString stream_id;
	QString room_id;
	QString server;
	QString key;
	// Optional account identity returned by a provider for this session. It is
	// not persisted and is useful when a manual profile did not supply a name.
	QString tiktok_username;

	[[nodiscard]] bool has_credentials() const
	{
		return !server.trimmed().isEmpty() && !key.trimmed().isEmpty();
	}
};

struct ProviderEndResult {
	bool ended = false;
	bool stale_session = false;
	QString error;
};

struct ProviderHeartbeatResult {
	bool session_is_live = false;
	QString error;
};
