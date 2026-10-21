#pragma once

#include <QValidator>
#include <QtQmlIntegration>

namespace Stride {

class IssueInputValidator : public QValidator {
	Q_OBJECT
	QML_ELEMENT
public:
	IssueInputValidator();

	State validate(QString&, int&) const override;
};

} // namespace Stride
