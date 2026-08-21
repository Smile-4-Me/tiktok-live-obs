// SPDX-License-Identifier: GPL-3.0-only

#include "secure_storage.hpp"

#include <QCoreApplication>
#include <QUuid>

#include <iostream>

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	const QString target = QStringLiteral("TikTokLiveObs/Test/%1")
		.arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
	const QByteArray expected(2048, 's');
	QString error;
	if (!SecureStorage::save(target, expected, QStringLiteral("TikTok Live OBS storage test"), &error)) {
		std::cerr << "Secure-storage write failed: " << error.toStdString() << '\n';
		return 1;
	}
	const QByteArray actual = SecureStorage::load(target, &error);
	const bool removed = SecureStorage::remove(target, &error);
	if (actual != expected) {
		std::cerr << "Secure-storage round-trip changed the value.\n";
		return 1;
	}
	if (!removed) {
		std::cerr << "Secure-storage cleanup failed: " << error.toStdString() << '\n';
		return 1;
	}
	return 0;
}
