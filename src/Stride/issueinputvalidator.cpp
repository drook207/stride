#include "issueinputvalidator.h"

namespace Stride {

IssueInputValidator::IssueInputValidator() { }

QValidator::State IssueInputValidator::validate(QString& input, int&) const
{
	QRegularExpression regex(R"((\d+)\:?.*)");
	QRegularExpressionMatch match = regex.match(input);
	if (match.hasMatch()) {
		return State::Acceptable;
	}

	return State::Intermediate;
}

} // namespace Stride
