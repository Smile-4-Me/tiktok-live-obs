// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "token_store.hpp"
#include "credential_chunks.hpp"
#include "secure_storage.hpp"

#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString storage_scope;
thread_local QString storage_error;

QString scoped_target_name(const QString &kind, const QString &profile_id)
{
	return QStringLiteral("TikTokLiveObs/%1/%2/%3")
		.arg(storage_scope, kind, profile_id);
}

QString target_name(const QString &profile_id)
{
	return scoped_target_name(QStringLiteral("Streamlabs"), profile_id);
}

QString live_credentials_target_name(const QString &profile_id)
{
	return scoped_target_name(QStringLiteral("LiveCredentials"), profile_id);
}

QString frame_signing_target_name(const QString &profile_id)
{
	return scoped_target_name(QStringLiteral("FrameSigning"), profile_id);
}

QString tiktok_studio_account_target_name(const QString &account_id)
{
	return scoped_target_name(QStringLiteral("TikTokStudioAccount"), account_id);
}

QString tiktok_studio_cookie_target_name(const QString &account_id, int index, int cookie_set)
{
	const QString kind = cookie_set == 0
		? QStringLiteral("TikTokStudioCookie%1").arg(index)
		: QStringLiteral("TikTokStudioCookieB%1").arg(index);
	return scoped_target_name(kind, account_id);
}

bool save_credential(const QString &target_name, const QByteArray &value, const QString &label)
{
	QString error;
	const bool saved = SecureStorage::save(target_name, value, label, &error);
	if (!saved)
		storage_error = error;
	return saved;
}

QByteArray load_credential(const QString &target_name)
{
	return SecureStorage::load(target_name);
}

void remove_credential(const QString &target_name)
{
	SecureStorage::remove(target_name);
}

} // namespace

void TokenStore::set_storage_scope(const QString &scope)
{
	storage_scope = scope;
}

bool TokenStore::save(const QString &profile_id, const QString &token)
{
	storage_error.clear();
	return save_credential(target_name(profile_id), token.toUtf8(),
		QStringLiteral("Streamlabs OAuth Token"));
}

QString TokenStore::load(const QString &profile_id)
{
	const QByteArray current = load_credential(target_name(profile_id));
	return QString::fromUtf8(current);
}

bool TokenStore::save_live_credentials(const QString &profile_id, const LiveCredentials &credentials)
{
	storage_error.clear();
	const QJsonObject object{{QStringLiteral("server"), credentials.server},
		{QStringLiteral("key"), credentials.key}};
	return save_credential(live_credentials_target_name(profile_id),
		QJsonDocument(object).toJson(QJsonDocument::Compact),
		QStringLiteral("TikTok LIVE credentials"));
}

LiveCredentials TokenStore::load_live_credentials(const QString &profile_id)
{
	const QByteArray value = load_credential(live_credentials_target_name(profile_id));
	const QJsonDocument document = QJsonDocument::fromJson(value);
	const QJsonObject object = document.object();
	return {object.value(QStringLiteral("server")).toString(), object.value(QStringLiteral("key")).toString()};
}

void TokenStore::remove_live_credentials(const QString &profile_id)
{
	remove_credential(live_credentials_target_name(profile_id));
}

bool TokenStore::save_frame_signing_credentials(const QString &profile_id,
	const FrameSigningCredentials &credentials)
{
	storage_error.clear();
	const QJsonObject object{
		{QStringLiteral("api_url"), credentials.api_url},
		{QStringLiteral("rapidapi_key"), credentials.rapidapi_key},
		{QStringLiteral("uid"), credentials.uid},
		{QStringLiteral("device_id"), credentials.device_id},
		{QStringLiteral("room_id"), credentials.room_id},
		{QStringLiteral("aid"), credentials.aid},
	};
	return save_credential(frame_signing_target_name(profile_id),
		QJsonDocument(object).toJson(QJsonDocument::Compact),
		QStringLiteral("TikTok frame signing credentials"));
}

FrameSigningCredentials TokenStore::load_frame_signing_credentials(const QString &profile_id)
{
	const QJsonObject object = QJsonDocument::fromJson(load_credential(frame_signing_target_name(profile_id))).object();
	FrameSigningCredentials credentials;
	const QString api_url = object.value(QStringLiteral("api_url")).toString().trimmed();
	if (!api_url.isEmpty())
		credentials.api_url = api_url;
	credentials.rapidapi_key = object.value(QStringLiteral("rapidapi_key")).toString();
	credentials.uid = object.value(QStringLiteral("uid")).toString();
	credentials.device_id = object.value(QStringLiteral("device_id")).toString();
	credentials.room_id = object.value(QStringLiteral("room_id")).toString();
	const QString aid = object.value(QStringLiteral("aid")).toString().trimmed();
	if (!aid.isEmpty())
		credentials.aid = aid;
	return credentials;
}

void TokenStore::remove_frame_signing_credentials(const QString &profile_id)
{
	remove_credential(frame_signing_target_name(profile_id));
}

bool TokenStore::save_tiktok_studio_account(const QString &account_id,
	const TikTokStudioAccountCredentials &credentials)
{
	storage_error.clear();
	if (account_id.trimmed().isEmpty())
		return false;

	const QByteArray previous_metadata = load_credential(tiktok_studio_account_target_name(account_id));
	const QJsonObject previous = QJsonDocument::fromJson(previous_metadata).object();
	TikTokStudioAccountCredentials stored = credentials;
	TikTokStudioAccountCredentials previous_identity;
	previous_identity.device_id = previous.value(QStringLiteral("device_id")).toString();
	previous_identity.install_id = previous.value(QStringLiteral("install_id")).toString();
	preserve_tiktok_studio_device_identity(previous_identity, &stored);
	const int previous_chunks = qBound(0,
		previous.value(QStringLiteral("cookie_chunks")).toInt(), CredentialChunks::maximum_chunks);
	const int previous_set = qBound(0, previous.value(QStringLiteral("cookie_set")).toInt(), 1);
	const int cookie_set = previous_metadata.isEmpty() ? 0 : 1 - previous_set;
	const int cookie_chunks = CredentialChunks::count(stored.cookie_jar.size());
	if (cookie_chunks > CredentialChunks::maximum_chunks) {
		storage_error = QStringLiteral("The TikTok cookie jar is too large for secure storage.");
		return false;
	}
	for (int index = 0; index < cookie_chunks; ++index) {
		const QByteArray chunk = CredentialChunks::at(stored.cookie_jar, index);
		if (!save_credential(tiktok_studio_cookie_target_name(account_id, index, cookie_set), chunk,
			QStringLiteral("TikTok LIVE Studio cookies"))) {
			for (int cleanup = 0; cleanup <= index; ++cleanup)
				remove_credential(tiktok_studio_cookie_target_name(account_id, cleanup, cookie_set));
			return false;
		}
	}

	const QJsonObject object{
		{QStringLiteral("rapidapi_key"), stored.rapidapi_key},
		{QStringLiteral("signer_api_url"), stored.signer_api_url},
		{QStringLiteral("device_id"), stored.device_id},
		{QStringLiteral("install_id"), stored.install_id},
		{QStringLiteral("live_studio_version"), stored.live_studio_version},
		{QStringLiteral("username"), stored.username},
		{QStringLiteral("user_id"), stored.user_id},
		{QStringLiteral("rapidapi_quota_limit"), stored.rapidapi_quota.limit},
		{QStringLiteral("rapidapi_quota_remaining"), stored.rapidapi_quota.remaining},
		{QStringLiteral("rapidapi_quota_reset"), stored.rapidapi_quota.reset_epoch_seconds},
		{QStringLiteral("rapidapi_quota_observed"), stored.rapidapi_quota.observed_epoch_seconds},
		{QStringLiteral("cookie_chunks"), cookie_chunks},
		{QStringLiteral("cookie_set"), cookie_set},
	};
	const QByteArray metadata = QJsonDocument(object).toJson(QJsonDocument::Compact);
	if (metadata.size() > SecureStorage::maximum_entry_bytes ||
		!save_credential(tiktok_studio_account_target_name(account_id), metadata,
			QStringLiteral("TikTok LIVE Studio account"))) {
		for (int index = 0; index < cookie_chunks; ++index)
			remove_credential(tiktok_studio_cookie_target_name(account_id, index, cookie_set));
		return false;
	}

	for (int index = 0; index < previous_chunks; ++index)
		remove_credential(tiktok_studio_cookie_target_name(account_id, index, previous_set));
	for (int index = cookie_chunks; index < CredentialChunks::maximum_chunks; ++index)
		remove_credential(tiktok_studio_cookie_target_name(account_id, index, cookie_set));
	return true;
}

TikTokStudioAccountCredentials TokenStore::load_tiktok_studio_account(const QString &account_id)
{
	TikTokStudioAccountCredentials credentials;
	if (account_id.trimmed().isEmpty())
		return credentials;
	const QJsonObject object = QJsonDocument::fromJson(
		load_credential(tiktok_studio_account_target_name(account_id))).object();
	credentials.rapidapi_key = object.value(QStringLiteral("rapidapi_key")).toString();
	const QString signer_api_url = object.value(QStringLiteral("signer_api_url")).toString().trimmed();
	if (!signer_api_url.isEmpty())
		credentials.signer_api_url = signer_api_url;
	credentials.device_id = object.value(QStringLiteral("device_id")).toString();
	credentials.install_id = object.value(QStringLiteral("install_id")).toString();
	const QString version = object.value(QStringLiteral("live_studio_version")).toString().trimmed();
	if (!version.isEmpty())
		credentials.live_studio_version = version;
	credentials.username = object.value(QStringLiteral("username")).toString();
	credentials.user_id = object.value(QStringLiteral("user_id")).toString();
	if (object.contains(QStringLiteral("rapidapi_quota_limit")))
		credentials.rapidapi_quota.limit = object.value(QStringLiteral("rapidapi_quota_limit")).toVariant().toLongLong();
	if (object.contains(QStringLiteral("rapidapi_quota_remaining")))
		credentials.rapidapi_quota.remaining = object.value(QStringLiteral("rapidapi_quota_remaining")).toVariant().toLongLong();
	if (object.contains(QStringLiteral("rapidapi_quota_reset")))
		credentials.rapidapi_quota.reset_epoch_seconds = object.value(QStringLiteral("rapidapi_quota_reset")).toVariant().toLongLong();
	if (object.contains(QStringLiteral("rapidapi_quota_observed")))
		credentials.rapidapi_quota.observed_epoch_seconds = object.value(QStringLiteral("rapidapi_quota_observed")).toVariant().toLongLong();
	const int cookie_chunks = qBound(0, object.value(QStringLiteral("cookie_chunks")).toInt(),
		CredentialChunks::maximum_chunks);
	const int cookie_set = qBound(0, object.value(QStringLiteral("cookie_set")).toInt(), 1);
	for (int index = 0; index < cookie_chunks; ++index) {
		const QByteArray chunk = load_credential(
			tiktok_studio_cookie_target_name(account_id, index, cookie_set));
		if (chunk.isEmpty()) {
			credentials.cookie_jar.clear();
			break;
		}
		credentials.cookie_jar += chunk;
	}
	return credentials;
}

void TokenStore::remove_tiktok_studio_account(const QString &account_id)
{
	if (account_id.trimmed().isEmpty())
		return;
	// Remove every possible chunk even if the metadata credential is missing or
	// damaged, so "Delete TikTok login" cannot leave orphaned cookies behind.
	for (int index = 0; index < CredentialChunks::maximum_chunks; ++index) {
		remove_credential(tiktok_studio_cookie_target_name(account_id, index, 0));
		remove_credential(tiktok_studio_cookie_target_name(account_id, index, 1));
	}
	remove_credential(tiktok_studio_account_target_name(account_id));
}

QString TokenStore::last_error()
{
	return storage_error;
}

void TokenStore::remove(const QString &profile_id)
{
	remove_credential(target_name(profile_id));
	remove_live_credentials(profile_id);
	remove_frame_signing_credentials(profile_id);
}
