#pragma once

#include <QDate>
#include <QJsonArray>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace QRedmineClient {

/*!
 * \brief Exports the time entries of a period as CSV or PDF, e.g. for invoicing.
 *
 * rows() combines the user's entries on Redmine (RedmineClient::getTimeEntryReport()) with the
 * local entries not on Redmine yet, like TimeOverview; without Redmine's entries it takes all
 * local ones. writeCsv() and writePdf() write them to a file.
 */
class TimeEntryExport : public QObject {
	Q_OBJECT
	QML_ELEMENT
	//! Why the last write failed, empty after one that succeeded.
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
public:
	explicit TimeEntryExport(QObject* parent = nullptr);

	QString lastError() const;

	/*!
	 * \brief rows The entries spent on the days from .. to, oldest first, as maps with "date"
	 * ("yyyy-MM-dd"), "issue_id", "project", "activity", "hours", "comments" and "pending" (not on
	 * Redmine yet)
	 * \param remoteEntries Redmine's entries for the period; ignored unless includesRemote
	 */
	Q_INVOKABLE QVariantList rows(
		const QJsonArray& remoteEntries, bool includesRemote, const QDate& from, const QDate& to) const;

	/*!
	 * \brief writeCsv Writes rows as CSV (UTF-8 with BOM, so spreadsheets detect it). Uses ';' and a
	 * decimal comma where the locale writes numbers that way (e.g. German), else ',' and a point.
	 */
	Q_INVOKABLE bool writeCsv(const QUrl& file, const QVariantList& rows);

	//! Writes rows as an A4 PDF table with a title and the total.
	Q_INVOKABLE bool writePdf(const QUrl& file, const QVariantList& rows, const QString& title);

signals:
	void lastErrorChanged();

private:
	void setLastError(const QString& error);
	QString mLastError;
};
}
