#include "timeentryexport.h"
#include "database.h"
#include <QFile>
#include <QJsonObject>
#include <QLocale>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>
#include <algorithm>

namespace QRedmineClient {

TimeEntryExport::TimeEntryExport(QObject* parent)
	: QObject(parent)
{
}

QString TimeEntryExport::lastError() const { return mLastError; }

void TimeEntryExport::setLastError(const QString& error)
{
	if (mLastError != error) {
		mLastError = error;
		emit lastErrorChanged();
	}
}

QVariantList TimeEntryExport::rows(
	const QJsonArray& remoteEntries, bool includesRemote, const QDate& from, const QDate& to) const
{
	QVariantList rows;
	if (includesRemote) {
		for (const QJsonValue& value : remoteEntries) {
			const QJsonObject entry = value.toObject();
			const QDate day = QDate::fromString(entry["spent_on"].toString(), Qt::ISODate);
			if (day < from || day > to) {
				continue;
			}
			rows.append(QVariantMap { { "date", day.toString(Qt::ISODate) },
				{ "issue_id", entry["issue"].toObject()["id"].toInt() },
				{ "project", entry["project"].toObject()["name"].toString() },
				{ "activity", entry["activity"].toObject()["name"].toString() }, { "hours", entry["hours"].toDouble() },
				{ "comments", entry["comments"].toString() }, { "pending", false } });
		}
	}
	for (const QJsonValue& value : Database::getEntriesBetween(from, to)) {
		const QJsonObject entry = value.toObject();
		const int status = entry["sync_status"].toInt();
		if (includesRemote && status != Database::Pending && status != Database::Rejected) {
			continue; // on Redmine, so among its entries
		}
		rows.append(QVariantMap { { "date", entry["spent_on"].toString().left(10) },
			{ "issue_id", entry["issue_id"].toInt() }, { "project", entry["project_name"].toString() },
			{ "activity", QString() }, { "hours", entry["hours"].toDouble() },
			{ "comments", entry["comments"].toString() }, { "pending", entry["redmine_id"].toInt() <= 0 } });
	}
	std::stable_sort(rows.begin(), rows.end(), [](const QVariant& a, const QVariant& b) {
		return a.toMap()["date"].toString() < b.toMap()["date"].toString();
	});
	return rows;
}

// A CSV field, quoted when it contains the separator, a quote or a line break.
static QString csvField(QString value, QChar separator)
{
	if (value.contains(separator) || value.contains('"') || value.contains('\n') || value.contains('\r')) {
		value.replace('"', QStringLiteral("\"\""));
		return '"' + value + '"';
	}
	return value;
}

bool TimeEntryExport::writeCsv(const QUrl& file, const QVariantList& rows)
{
	QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		setLastError(out.errorString());
		return false;
	}
	const QLocale locale;
	const bool decimalComma = locale.decimalPoint() == QStringLiteral(",");
	const QChar separator = decimalComma ? ';' : ',';
	auto hours = [&](double value) {
		QString text = QString::number(value, 'f', 2);
		return decimalComma ? text.replace('.', ',') : text;
	};

	QStringList lines;
	lines << QStringList { tr("Date"), tr("Issue"), tr("Project"), tr("Activity"), tr("Hours"), tr("Comment") }.join(
		separator);
	for (const QVariant& value : rows) {
		const QVariantMap row = value.toMap();
		// Project-level entries (e.g. meetings) have no issue.
		const int issueId = row["issue_id"].toInt();
		lines << QStringList { row["date"].toString(), issueId > 0 ? QString::number(issueId) : QString(),
			csvField(row["project"].toString(), separator), csvField(row["activity"].toString(), separator),
			hours(row["hours"].toDouble()), csvField(row["comments"].toString(), separator) }
					 .join(separator);
	}
	out.write("\xEF\xBB\xBF"); // UTF-8 byte order mark
	out.write((lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8());
	setLastError(QString());
	return true;
}

bool TimeEntryExport::writePdf(const QUrl& file, const QVariantList& rows, const QString& title)
{
	const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
	QFile probe(path);
	if (!probe.open(QIODevice::WriteOnly)) {
		setLastError(probe.errorString());
		return false;
	}
	probe.close();

	const QLocale locale;
	double total = 0;
	QString html = QStringLiteral("<h2>%1</h2><table width=\"100%\" cellpadding=\"4\" border=\"1\" "
								  "style=\"border-collapse: collapse\"><tr>")
					   .arg(title.toHtmlEscaped());
	for (const QString& heading :
		{ tr("Date"), tr("Issue"), tr("Project"), tr("Activity"), tr("Comment"), tr("Hours") }) {
		html += QStringLiteral("<th align=\"left\">%1</th>").arg(heading);
	}
	html += QStringLiteral("</tr>");
	for (const QVariant& value : rows) {
		const QVariantMap row = value.toMap();
		total += row["hours"].toDouble();
		const int issueId = row["issue_id"].toInt();
		html += QStringLiteral(
			"<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td align=\"right\">%6</td></tr>")
					.arg(locale.toString(QDate::fromString(row["date"].toString(), Qt::ISODate), QLocale::ShortFormat))
					.arg(issueId > 0 ? QStringLiteral("#%1").arg(issueId) : QString())
					.arg(row["project"].toString().toHtmlEscaped(), row["activity"].toString().toHtmlEscaped(),
						row["comments"].toString().toHtmlEscaped(), locale.toString(row["hours"].toDouble(), 'f', 2));
	}
	html += QStringLiteral("<tr><td colspan=\"5\"><b>%1</b></td><td align=\"right\"><b>%2</b></td></tr></table>")
				.arg(tr("Total"), locale.toString(total, 'f', 2));

	QPdfWriter writer(path);
	writer.setPageSize(QPageSize(QPageSize::A4));
	writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
	writer.setTitle(title);
	QTextDocument document;
	document.setHtml(html);
	document.print(&writer);
	setLastError(QString());
	return true;
}
}
