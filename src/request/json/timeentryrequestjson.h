#pragma once

#include "request/json/jsonrequest.h"
#include "types/timeentry.h"

namespace QRedmine::Requests::Json {

/**
 * @class TimeEntryRequestJson
 * @brief Manages the JSON request and response for time entry operations.
 *
 * This class is responsible for creating, validating, and handling time entry data
 * in JSON format for sending and receiving through the network. It extends from the
 * JsonRequest class and implements the necessary JSON-specific functionalities.
 *
 * @sa JsonRequest
 */
class TimeEntryRequestJson final : public JsonRequest {
public:
	explicit TimeEntryRequestJson(const Types::TimeEntry& entry, QObject* parent = nullptr);

	// Copy constructor
	TimeEntryRequestJson(const TimeEntryRequestJson& other);

	// Move constructor
	TimeEntryRequestJson(TimeEntryRequestJson&& other) noexcept;

	static QSharedPointer<TimeEntryRequestJson> createFromIssueId(
		qint32 issueId, const QStringView& comment, const double hours, const QDate& spentOn);
	static QSharedPointer<TimeEntryRequestJson> createFromProjectId(
		qint32 porjectId, const QStringView& comment, const double hours, const QDate& spentOn);
	// Request interface

private:
	QSharedDataPointer<Types::TimeEntry> mEntry {};

public:
	bool isValidToSend() override;
private slots:
	void requestFinished() override;

	// JsonRequest interface
public:
	bool toJson() override;
	[[nodiscard]] bool fromJson() override;
	[[nodiscard]] QSharedDataPointer<Types::TimeEntry> Entry() const;
	void setEntry(QSharedDataPointer<Types::TimeEntry> newEntry);
};

} // namespace QRedmine

Q_DECLARE_TYPEINFO(QRedmine::Requests::Json::TimeEntryRequestJson, Q_COMPLEX_TYPE);
