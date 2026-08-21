// SPDX-License-Identifier: GPL-3.0-only

#include "credential_chunks.hpp"

#include <iostream>

int main()
{
	const QByteArray cookie_jar(4000, 'x');
	const int chunks = CredentialChunks::count(cookie_jar.size());
	if (chunks != 2) {
		std::cerr << "A 4,000-byte cookie jar was not split into two entries.\n";
		return 1;
	}
	QByteArray rebuilt;
	for (int index = 0; index < chunks; ++index) {
		const QByteArray chunk = CredentialChunks::at(cookie_jar, index);
		if (chunk.size() > SecureStorage::maximum_entry_bytes) {
			std::cerr << "A cookie chunk exceeds the native secure-storage limit.\n";
			return 1;
		}
		rebuilt += chunk;
	}
	if (rebuilt != cookie_jar) {
		std::cerr << "Cookie chunks did not round-trip losslessly.\n";
		return 1;
	}
	return 0;
}
