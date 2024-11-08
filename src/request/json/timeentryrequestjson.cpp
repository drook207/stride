#include "timeentryrequestjson.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <utility>

namespace QRedmine::Requests::Json {
Q_LOGGING_CATEGORY(lcTimeEntryJson, "qredmine.timeentryjson")

TimeEntryRequestJson::TimeEntryRequestJson(const Types::TimeEntry& entry, QObject* parent)
	: JsonRequest(parent)
	, mEntry(entry)

{
	mResource = QStringLiteral("time_entries");

	if (TimeEntryRequestJson::toJson()) {
		mRequest.setRawHeader("Content-Length", QByteArray::number(mData.toJson().size()));
		resourceRequestMode();
		mPostData = mData.toJson();
		return;
	}
	mMode = QNetworkAccessManager::GetOperation;
}

// Implementation of the copy and move constructors
TimeEntryRequestJson::TimeEntryRequestJson(const TimeEntryRequestJson& other)
	: JsonRequest(other.parent())
	, mEntry(other.mEntry)
{
	mResource = other.mResource;
	mMode = other.mMode;
	mIsValidToSend = other.mIsValidToSend;
	mData = other.mData;
}

TimeEntryRequestJson::TimeEntryRequestJson(TimeEntryRequestJson&& other) noexcept
	: JsonRequest(other.parent())
	, mEntry(std::move(other.mEntry))
{
	mResource = std::move(other.mResource);
	mMode = other.mMode;
	mIsValidToSend = other.mIsValidToSend;
	mData = std::move(other.mData);
	other.mIsValidToSend = false;
}

QSharedPointer<TimeEntryRequestJson> TimeEntryRequestJson::createFromIssueId(qint32 issueId, const QStringView &comment, const double hours, const QDate &spentOn)
{

}

QSharedDataPointer<Types::TimeEntry> TimeEntryRequestJson::Entry() const
{
	return mEntry;
}

void TimeEntryRequestJson::setEntry(QSharedDataPointer<Types::TimeEntry> newEntry)
{
	mEntry = newEntry;
}

bool TimeEntryRequestJson::isValidToSend()
{
	return mIsValidToSend; // NOTE Maybe do more checks
}

void TimeEntryRequestJson::requestFinished()
{
	const QByteArray rawData = mReply->readAll();
	mData = QJsonDocument::fromJson(rawData);
	if (fromJson() && mReply->error() == QNetworkReply::NoError) {
		return emit finished();
	}
	return emit error();
}

bool TimeEntryRequestJson::toJson()
{
	if (mEntry.hours < 0.01f) {
		qDebug(lcTimeEntryJson) << "Time entry has to be at least 36 seconds";
		return false;
	}

	if (mEntry.issue.id == Global::DEFAULT_ID && mEntry.project.id == Global::DEFAULT_ID) {
		qDebug(lcTimeEntryJson) << "Either issue id or project id must be specified.";
		return false;
	}

	QJsonObject attributes;

	attributes["hours"] = mEntry.hours;

	if (mEntry.activity.id != Global::DEFAULT_ID) {
		attributes["activity_id"] = mEntry.activity.id;
	}

	if (!mEntry.comment.isEmpty()) {
		attributes["comments"] = mEntry.comment;
	}

	if (mEntry.issue.id != Global::DEFAULT_ID) {
		attributes["issue_id"] = mEntry.issue.id;
	}

	if (mEntry.project.id != Global::DEFAULT_ID) {
		attributes["project_id"] = mEntry.project.id;
	}

	if (mEntry.spentOn.isValid()) {
		attributes["spent_on"] = mEntry.spentOn.toString(Qt::ISODate);
	}

	if (!mEntry.customFields.isEmpty()) {
		QJsonArray customFields;

		for (const auto& [id, name, values, possibleValues, defaultValue, type, format, regex, minLength, maxLength,
				 allProjects, isRequired, isFilter, searchable, multiple, visible, projects, trackers] :
			std::as_const(mEntry.customFields)) {
			QJsonObject customField;
			customField["id"] = id;
			customField["value"] = values.at(0);
			customFields.append(customField);
		}

		attributes["custom_fields"] = customFields;
	}

	QJsonObject data;
	data["time_entry"] = attributes;

	mData.setObject(data);
	mIsValidToSend = true;
	return true;
}

bool TimeEntryRequestJson::fromJson()
{
	if (mData.isEmpty() || mData.isNull()) {
		qDebug(lcTimeEntryJson) << "Received JSON Object is empty or null.";
		return false;
	}

	mEntry.hours = mData["hours"].toDouble();

	mEntry.activity.id = mData["activity_id"].toInt();

	mEntry.comment = mData["comments"].toString();

	mEntry.issue.id = mData["issue_id"].toInt();

	mEntry.project.id = mData["project_id"].toInt();

	mEntry.spentOn = mData["spent_on"].toVariant().toDate();

	return true;
}

} // namespace QRedmine
