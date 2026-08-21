// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "token_store.hpp"

#include <QUuid>

#include <iostream>

int main()
{
	const QString scope = QStringLiteral("token-store-smoke-%1")
		.arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
	const QString profile_id = QStringLiteral("profile-%1")
		.arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
	TokenStore::set_storage_scope(scope);

	// This unique scope guarantees that the test can neither read nor remove a
	// user's existing credentials. Cleanup always targets only this test record.
	struct Cleanup {
		QString profile_id;
		~Cleanup() { TokenStore::remove(profile_id); }
	} cleanup{profile_id};

	if (!TokenStore::save(profile_id, QStringLiteral("temporary-token"))) {
		std::cerr << "Could not save the temporary token.\n";
		return 1;
	}
	if (TokenStore::load(profile_id) != QStringLiteral("temporary-token")) {
		std::cerr << "Stored token did not round-trip.\n";
		return 2;
	}

	const LiveCredentials expected{QStringLiteral("rtmp://127.0.0.1/live"),
		QStringLiteral("temporary-stream-key")};
	if (!TokenStore::save_live_credentials(profile_id, expected)) {
		std::cerr << "Could not save temporary live credentials.\n";
		return 3;
	}
	const LiveCredentials actual = TokenStore::load_live_credentials(profile_id);
	if (actual.server != expected.server || actual.key != expected.key) {
		std::cerr << "Live credentials did not round-trip.\n";
		return 4;
	}

	TokenStore::remove(profile_id);
	if (!TokenStore::load(profile_id).isEmpty() ||
		!TokenStore::load_live_credentials(profile_id).server.isEmpty() ||
		!TokenStore::load_live_credentials(profile_id).key.isEmpty()) {
		std::cerr << "Temporary credentials remained after cleanup.\n";
		return 5;
	}

	std::cout << "Token Store smoke test passed.\n";
	return 0;
}
