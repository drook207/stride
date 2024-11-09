#pragma once

#include "qredmineclient_global.h"
#include "types/timeentry.h"
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequestFactory>
#include <QObject>
#include <QRestAccessManager>
#include <QStringView>
#include <memory>
#include <qtypes.h>

namespace QRedmine {
/**
 * @brief
 *
 */
class QREDMINECLIENT_EXPORT RestClient final : public QObject {
	Q_OBJECT
public:
	/**
	 * @brief Construct a new Client object
	 *
	 * @param url
	 * @param apiKey
	 * @param sslVerify
	 * @param parent
	 */
	RestClient(QUrl url, const QStringView& apiKey, bool sslVerify = true, QObject* parent = nullptr);

	/**
	 * @brief
	 *
	 * @return QUrl
	 */
	[[nodiscard]] QUrl Url() const;
	/**
	 * @brief Set the Url object
	 *
	 * @param newUrl
	 */
	void setUrl(const QUrl& newUrl);

	/**
	 * @brief
	 *
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool SslVerify() const;
	/**
	 * @brief Set the Ssl Verify object
	 *
	 * @param newSslVerify
	 */
	void setSslVerify(bool newSslVerify);

	/**
	 * @brief Get the Time Entries object
	 *
	 * @param id
	 * @param callback
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool getTimeEntrieFromIssueId(const QStringView& id, std::function<void(QRestReply&)> callback);

	/**
	 * @brief Get the Time Entry From Project Id object
	 *
	 * @param id
	 * @param callback
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool getTimeEntryFromProjectId(const QStringView& id, std::function<void(QRestReply&)> callback);
	/**
	 * @brief Get the Time Entries object
	 *
	 * @param callback
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool getTimeEntries(std::function<void(QRestReply&)> callback);
	/**
	 * @brief Get the Time Entries With Filter object
	 *
	 * @param id
	 * @param from
	 * @param to
	 * @param callback
	 * @param limit
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool getTimeEntriesWithFilter(const QStringView& id, const QDate& from, const QDate& to,
		std::function<void(QRestReply&)> callback, quint8 limit = 100);
	/**
	 * @brief Create a Time Entry object
	 *
	 * @param entry
	 * @param callback
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool createTimeEntry(const Types::TimeEntry& entry, std::function<void(QRestReply&)> callback);

	[[nodiscard]] bool updateTimeEntry(const Types::TimeEntry& entry, std::function<void(QRestReply&)> callback);

public slots:

	/**
	 * @brief
	 *
	 * @param reply
	 * @param errors
	 */
	void handleSslError(QNetworkReply* reply, const QList<QSslError>& errors) const;

private:
	QNetworkAccessManager* mNetworkManager {};
	QNetworkRequestFactory mFactory {};
	std::unique_ptr<QRestAccessManager> mRestAccesManager {};
	QUrl mUrl {};
	bool mSslVerify = true;

private slots:
	void replyFinished();
};
} // namespace QRedmine
