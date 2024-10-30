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

	Types::TimeEntry mEntry {};
	// Request interface

public:
	bool isValidToSend() override;
private slots:
	void requestFinished() override;

	// JsonRequest interface
public:
	bool toJson() override;
	[[nodiscard]] bool fromJson() override;
};

} // namespace QRedmine
