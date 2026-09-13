#pragma once

#include "Utils_global.h"
#include <QObject>
#include <QString>
#include <functional>

namespace QRedmineClient {
namespace Utils {

	/*!
	 * \brief ReadSecret Asynchronously reads a secret from the OS-native secure storage (e.g. Secret
	 * Service/KWallet on Linux, Credential Manager on Windows, Keychain on macOS). This is
	 * fire-and-forget: the underlying job deletes itself once it has finished, and none of the
	 * callbacks are invoked before this function returns.
	 * \param service The keychain service identifier the secret was stored under.
	 * \param key The key identifying the secret within service.
	 * \param onSuccess Called with the secret's value once it has been read successfully.
	 * \param onMissing Called when no secret is stored for service/key. Not treated as an error.
	 * \param onError Called with a human-readable error message on any other failure, including when
	 * no secure backend is available. This never falls back to insecure storage.
	 * \param context Optional. When given, none of the callbacks are invoked once context has been
	 * destroyed (analogous to the context object overload of QObject::connect()). Pass the object
	 * whose lifetime the callbacks' captures depend on (e.g. a captured `this`) to avoid a
	 * use-after-free if that object can be destroyed before the async job completes.
	 */
	UTILS_EXPORT void ReadSecret(const QString& service, const QString& key,
		const std::function<void(const QString&)>& onSuccess, const std::function<void()>& onMissing = {},
		const std::function<void(const QString&)>& onError = {}, QObject* context = nullptr);

	/*!
	 * \brief WriteSecret Asynchronously writes a secret to the OS-native secure storage. Fire-and-forget:
	 * the underlying job deletes itself once it has finished.
	 * \param service The keychain service identifier to store the secret under.
	 * \param key The key identifying the secret within service.
	 * \param value The secret value to store.
	 * \param onError Called with a human-readable error message on failure.
	 * \param context Optional, see ReadSecret().
	 */
	UTILS_EXPORT void WriteSecret(const QString& service, const QString& key, const QString& value,
		const std::function<void(const QString&)>& onError = {}, QObject* context = nullptr);

	/*!
	 * \brief DeleteSecret Asynchronously deletes a secret from the OS-native secure storage. Fire-and-forget:
	 * the underlying job deletes itself once it has finished.
	 * \param service The keychain service identifier the secret was stored under.
	 * \param key The key identifying the secret within service.
	 * \param onError Called with a human-readable error message on failure.
	 * \param context Optional, see ReadSecret().
	 */
	UTILS_EXPORT void DeleteSecret(const QString& service, const QString& key,
		const std::function<void(const QString&)>& onError = {}, QObject* context = nullptr);

} // namespace Utils
} // namespace QRedmineClient
