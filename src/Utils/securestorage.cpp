#include "securestorage.h"

#include <QLoggingCategory>
#include <qtkeychain/keychain.h>

Q_LOGGING_CATEGORY(lcSecureStorage, "qredmine.utils.securestorage")

namespace QRedmineClient {
namespace Utils {

	void ReadSecret(const QString& service, const QString& key, const std::function<void(const QString&)>& onSuccess,
		const std::function<void()>& onMissing, const std::function<void(const QString&)>& onError, QObject* context)
	{
		auto* job = new QKeychain::ReadPasswordJob(service);
		job->setAutoDelete(true);
		job->setKey(key);
		// Never silently fall back to plaintext storage, see setInsecureFallback() documentation.
		job->setInsecureFallback(false);

		// Use context (when given) as the connection's context object so the callbacks -- which may
		// capture and use e.g. `this` of the caller -- are never invoked once that object is destroyed.
		QObject* connectionContext = context ? context : static_cast<QObject*>(job);
		QObject::connect(
			job, &QKeychain::Job::finished, connectionContext, [onSuccess, onMissing, onError](QKeychain::Job* job) {
				auto* readJob = qobject_cast<QKeychain::ReadPasswordJob*>(job);
				if (readJob->error() == QKeychain::NoError) {
					if (onSuccess) {
						onSuccess(readJob->textData());
					}
				} else if (readJob->error() == QKeychain::EntryNotFound) {
					if (onMissing) {
						onMissing();
					}
				} else {
					qCWarning(lcSecureStorage)
						<< "Failed to read secret from secure storage:" << readJob->errorString();
					if (onError) {
						onError(readJob->errorString());
					}
				}
			});

		job->start();
	}

	void WriteSecret(const QString& service, const QString& key, const QString& value,
		const std::function<void(const QString&)>& onError, QObject* context)
	{
		auto* job = new QKeychain::WritePasswordJob(service);
		job->setAutoDelete(true);
		job->setKey(key);
		job->setInsecureFallback(false);
		job->setTextData(value);

		QObject* connectionContext = context ? context : static_cast<QObject*>(job);
		QObject::connect(job, &QKeychain::Job::finished, connectionContext, [onError](QKeychain::Job* job) {
			if (job->error() != QKeychain::NoError) {
				qCWarning(lcSecureStorage) << "Failed to write secret to secure storage:" << job->errorString();
				if (onError) {
					onError(job->errorString());
				}
			}
		});

		job->start();
	}

	void DeleteSecret(const QString& service, const QString& key, const std::function<void(const QString&)>& onError,
		QObject* context)
	{
		auto* job = new QKeychain::DeletePasswordJob(service);
		job->setAutoDelete(true);
		job->setKey(key);
		job->setInsecureFallback(false);

		QObject* connectionContext = context ? context : static_cast<QObject*>(job);
		QObject::connect(job, &QKeychain::Job::finished, connectionContext, [onError](QKeychain::Job* job) {
			if (job->error() != QKeychain::NoError && job->error() != QKeychain::EntryNotFound) {
				qCWarning(lcSecureStorage) << "Failed to delete secret from secure storage:" << job->errorString();
				if (onError) {
					onError(job->errorString());
				}
			}
		});

		job->start();
	}

} // namespace Utils
} // namespace QRedmineClient
