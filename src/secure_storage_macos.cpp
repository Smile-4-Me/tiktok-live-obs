// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "secure_storage.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <QVector>

namespace {

class CfRef final {
public:
	explicit CfRef(CFTypeRef value = nullptr) : value_(value) {}
	~CfRef() { if (value_) CFRelease(value_); }
	CfRef(const CfRef &) = delete;
	CfRef &operator=(const CfRef &) = delete;
	CFTypeRef get() const { return value_; }
	CFTypeRef release() { CFTypeRef value = value_; value_ = nullptr; return value; }
private:
	CFTypeRef value_;
};

CFStringRef cf_string(const QString &value)
{
	const QByteArray utf8 = value.toUtf8();
	return CFStringCreateWithBytes(kCFAllocatorDefault,
		reinterpret_cast<const UInt8 *>(utf8.constData()), utf8.size(),
		kCFStringEncodingUTF8, false);
}

QString keychain_error(OSStatus status)
{
	CfRef message(SecCopyErrorMessageString(status, nullptr));
	if (!message.get())
		return QStringLiteral("macOS Keychain error %1").arg(status);
	const CFStringRef string = static_cast<CFStringRef>(message.get());
	const CFIndex length = CFStringGetLength(string);
	QVector<UniChar> characters(length);
	CFStringGetCharacters(string, CFRangeMake(0, length), characters.data());
	return QString::fromUtf16(reinterpret_cast<const char16_t *>(characters.constData()), length);
}

void set_error(QString *error, OSStatus status)
{
	if (error)
		*error = keychain_error(status);
}

CFMutableDictionaryRef base_query(const QString &target)
{
	auto *query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
	CfRef service(cf_string(QStringLiteral("TikTok Live OBS")));
	CfRef account(cf_string(target));
	CFDictionarySetValue(query, kSecAttrService, service.get());
	CFDictionarySetValue(query, kSecAttrAccount, account.get());
	return query;
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
	CfRef query(base_query(target));
	CfRef data(CFDataCreate(kCFAllocatorDefault,
		reinterpret_cast<const UInt8 *>(value.constData()), value.size()));
	CfRef label_string(cf_string(label));
	CfRef updates(CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	auto *update_dictionary = reinterpret_cast<CFMutableDictionaryRef>(
		const_cast<void *>(updates.get()));
	CFDictionarySetValue(update_dictionary, kSecValueData, data.get());
	CFDictionarySetValue(update_dictionary, kSecAttrLabel, label_string.get());
	OSStatus status = SecItemUpdate(static_cast<CFDictionaryRef>(query.get()), update_dictionary);
	if (status == errSecItemNotFound) {
		auto *add_query = reinterpret_cast<CFMutableDictionaryRef>(
			const_cast<void *>(query.get()));
		CFDictionarySetValue(add_query, kSecValueData, data.get());
		CFDictionarySetValue(add_query, kSecAttrLabel, label_string.get());
		status = SecItemAdd(add_query, nullptr);
	}
	if (status == errSecSuccess)
		return true;
	set_error(error, status);
	return false;
}

QByteArray SecureStorage::load(const QString &target, QString *error)
{
	if (error)
		error->clear();
	CfRef query(base_query(target));
	auto *dictionary = reinterpret_cast<CFMutableDictionaryRef>(
		const_cast<void *>(query.get()));
	CFDictionarySetValue(dictionary, kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(dictionary, kSecMatchLimit, kSecMatchLimitOne);
	CFTypeRef result = nullptr;
	const OSStatus status = SecItemCopyMatching(dictionary, &result);
	CfRef result_data(result);
	if (status == errSecItemNotFound)
		return {};
	if (status != errSecSuccess || !result || CFGetTypeID(result) != CFDataGetTypeID()) {
		set_error(error, status);
		return {};
	}
	const auto *data = static_cast<CFDataRef>(result);
	return QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), CFDataGetLength(data));
}

bool SecureStorage::remove(const QString &target, QString *error)
{
	if (error)
		error->clear();
	CfRef query(base_query(target));
	const OSStatus status = SecItemDelete(static_cast<CFDictionaryRef>(query.get()));
	if (status == errSecSuccess || status == errSecItemNotFound)
		return true;
	set_error(error, status);
	return false;
}
