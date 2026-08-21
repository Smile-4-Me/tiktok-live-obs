// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "token_store.hpp"

#include <windows.h>
#include <wincred.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString storage_scope;

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

bool save_credential(const QString &target_name, const QByteArray &value, const wchar_t *username)
{
	const std::wstring target = target_name.toStdWString();
	CREDENTIALW credential = {};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<wchar_t *>(target.c_str());
	credential.CredentialBlobSize = static_cast<DWORD>(value.size());
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(value.constData()));
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	credential.UserName = const_cast<wchar_t *>(username);
	return CredWriteW(&credential, 0) == TRUE;
}

QByteArray load_credential(const QString &target_name)
{
	const std::wstring target = target_name.toStdWString();
	PCREDENTIALW credential = nullptr;
	if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) || !credential)
		return {};
	const QByteArray value(reinterpret_cast<const char *>(credential->CredentialBlob),
		static_cast<int>(credential->CredentialBlobSize));
	CredFree(credential);
	return value;
}

void remove_credential(const QString &target_name)
{
	const std::wstring target = target_name.toStdWString();
	CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

} // namespace

void TokenStore::set_storage_scope(const QString &scope)
{
	storage_scope = scope;
}

bool TokenStore::save(const QString &profile_id, const QString &token)
{
	return save_credential(target_name(profile_id), token.toUtf8(), L"Streamlabs OAuth Token");
}

QString TokenStore::load(const QString &profile_id)
{
	const QByteArray current = load_credential(target_name(profile_id));
	return QString::fromUtf8(current);
}

bool TokenStore::save_live_credentials(const QString &profile_id, const LiveCredentials &credentials)
{
	const QJsonObject object{{QStringLiteral("server"), credentials.server},
		{QStringLiteral("key"), credentials.key}};
	return save_credential(live_credentials_target_name(profile_id),
		QJsonDocument(object).toJson(QJsonDocument::Compact), L"TikTok LIVE credentials");
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

void TokenStore::remove(const QString &profile_id)
{
	remove_credential(target_name(profile_id));
	remove_live_credentials(profile_id);
}
