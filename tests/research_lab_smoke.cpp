// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "research_lab.hpp"

#include <QCoreApplication>
#include <QTimer>

#include <iostream>

int main(int argc, char *argv[])
{
	QCoreApplication application(argc, argv);
	ResearchLab lab;
	bool heartbeat_confirmed = false;
	bool heartbeat_failed = false;
	QString failure_detail;
	QString callback_session;

	lab.set_status_callback([&](const QString &session_id, bool healthy, const QString &detail) {
		callback_session = session_id;
		heartbeat_confirmed = healthy;
		heartbeat_failed = !healthy;
		failure_detail = detail;
		application.quit();
	});

	QString start_error;
	if (!lab.start(QStringLiteral("research-smoke-first"), &start_error)) {
		std::cerr << "Could not start local research harness: "
			<< start_error.toStdString() << '\n';
		return 1;
	}
	// Begin a replacement session before the first reply is guaranteed to have
	// arrived. This exercises the generation guard for late asynchronous replies.
	lab.stop();
	if (!lab.start(QStringLiteral("research-smoke-second"), &start_error)) {
		std::cerr << "Could not start replacement local research harness: "
			<< start_error.toStdString() << '\n';
		return 1;
	}

	QTimer::singleShot(5000, &application, &QCoreApplication::quit);
	application.exec();

	if (!heartbeat_confirmed || heartbeat_failed) {
		std::cerr << "Local heartbeat did not complete successfully";
		if (!failure_detail.isEmpty())
			std::cerr << ": " << failure_detail.toStdString();
		std::cerr << '\n';
		return 2;
	}
	if (callback_session != QStringLiteral("research-smoke-second")) {
		std::cerr << "A stale heartbeat altered the replacement session.\n";
		return 4;
	}

	lab.stop();
	if (lab.active()) {
		std::cerr << "Local research harness remained active after stop.\n";
		return 3;
	}

	std::cout << "Local research harness smoke test passed.\n";
	return 0;
}
