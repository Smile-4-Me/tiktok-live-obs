// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "secure_storage.hpp"

#include <windows.h>
#include <wincred.h>

static_assert(SecureStorage::maximum_entry_bytes <= CRED_MAX_CREDENTIAL_BLOB_SIZE);

namespace {

QString windows_error(DWORD code)
{
	wchar_t *message = nullptr;
	const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, code, 0, reinterpret_cast<wchar_t *>(&message), 0, nullptr);
	QString result = length && message
		? QString::fromWCharArray(message, static_cast<qsizetype>(length)).trimmed()
		: QStringLiteral("Windows error %1").arg(code);
	if (message)
		LocalFree(message);
	return result;
}

void set_error(QString *error, const QString &message)
{
	if (error)
		*error = message;
}

} // namespace

bool SecureStorage::save(const QString &target_name, const QByteArray &value,
	const QString &label, QString *error)
{
	if (error)
		error->clear();
	if (value.size() > maximum_entry_bytes) {
		set_error(error, QStringLiteral("The secure-storage entry is %1 bytes; Windows allows at most %2 bytes.")
			.arg(value.size()).arg(maximum_entry_bytes));
		return false;
	}

	const std::wstring target = target_name.toStdWString();
	const std::wstring username = label.toStdWString();
	CREDENTIALW credential = {};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<wchar_t *>(target.c_str());
	credential.CredentialBlobSize = static_cast<DWORD>(value.size());
	credential.CredentialBlob = value.isEmpty()
		? nullptr
		: reinterpret_cast<LPBYTE>(const_cast<char *>(value.constData()));
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	credential.UserName = const_cast<wchar_t *>(username.c_str());
	if (CredWriteW(&credential, 0) == TRUE)
		return true;
	set_error(error, windows_error(GetLastError()));
	return false;
}

QByteArray SecureStorage::load(const QString &target_name, QString *error)
{
	if (error)
		error->clear();
	const std::wstring target = target_name.toStdWString();
	PCREDENTIALW credential = nullptr;
	if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) || !credential) {
		const DWORD code = GetLastError();
		if (code != ERROR_NOT_FOUND)
			set_error(error, windows_error(code));
		return {};
	}
	const QByteArray value(reinterpret_cast<const char *>(credential->CredentialBlob),
		static_cast<qsizetype>(credential->CredentialBlobSize));
	CredFree(credential);
	return value;
}

bool SecureStorage::remove(const QString &target_name, QString *error)
{
	if (error)
		error->clear();
	const std::wstring target = target_name.toStdWString();
	if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) == TRUE)
		return true;
	const DWORD code = GetLastError();
	if (code == ERROR_NOT_FOUND)
		return true;
	set_error(error, windows_error(code));
	return false;
}
