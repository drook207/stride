#pragma once

#include "request/json/jsonrequest.h"
#include "types/timeentry.h"

namespace QRedmine {

class TimeEntryRequestJson : public JsonRequest {
public:
	TimeEntryRequestJson(TimeEntry entry = TimeEntry(), QObject* parent = nullptr);

private:
	TimeEntry mEntry {};
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
