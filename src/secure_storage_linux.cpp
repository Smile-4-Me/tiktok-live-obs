// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "secure_storage.hpp"

// Qt's keyword compatibility macro otherwise rewrites GDBusSignalInfo::signals
// while libsecret pulls in GLib's D-Bus declarations.
#ifdef signals
#undef signals
#endif
#include <libsecret/secret.h>

namespace {

const SecretSchema storage_schema = {
	"com.loukious.TikTokLiveObs",
	SECRET_SCHEMA_NONE,
	{
		{"target", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
	},
};

void take_error(QString *destination, GError *error)
{
	if (destination && error)
		*destination = QString::fromUtf8(error->message);
	if (error)
		g_error_free(error);
}

} // namespace

bool SecureStorage::save(const QString &target, const QByteArray &value,
	const QString &label, QString *error)
{
	if (error)
		error->clear();
	if (value.size() > maximum_entry_bytes) {
		if (error)
			*error = QStringLiteral("The secure-storage entry exceeds the portable %1-byte limit.")
				.arg(maximum_entry_bytes);
		return false;
	}
	const QByteArray target_utf8 = target.toUtf8();
	const QByteArray label_utf8 = label.toUtf8();
	const QByteArray encoded = value.toBase64(QByteArray::Base64Encoding);
	GError *native_error = nullptr;
	const gboolean stored = secret_password_store_sync(&storage_schema,
		SECRET_COLLECTION_DEFAULT, label_utf8.constData(), encoded.constData(),
		nullptr, &native_error, "target", target_utf8.constData(), nullptr);
	take_error(error, native_error);
	return stored == TRUE;
}

QByteArray SecureStorage::load(const QString &target, QString *error)
{
	if (error)
		error->clear();
	const QByteArray target_utf8 = target.toUtf8();
	GError *native_error = nullptr;
	gchar *encoded = secret_password_lookup_sync(&storage_schema, nullptr,
		&native_error, "target", target_utf8.constData(), nullptr);
	take_error(error, native_error);
	if (!encoded)
		return {};
	const QByteArray value = QByteArray::fromBase64(QByteArray(encoded),
		QByteArray::Base64Encoding);
	secret_password_free(encoded);
	return value;
}

bool SecureStorage::remove(const QString &target, QString *error)
{
	if (error)
		error->clear();
	const QByteArray target_utf8 = target.toUtf8();
	GError *native_error = nullptr;
	const gboolean removed = secret_password_clear_sync(&storage_schema, nullptr,
		&native_error, "target", target_utf8.constData(), nullptr);
	take_error(error, native_error);
	return removed == TRUE;
}
