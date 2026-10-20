#include "logging.h"

#include <QMessageLogger>

namespace QRedmineClient::Utils::Logging {

namespace {
	// Fields of the message being logged by this thread, picked up by the message handler during the same call
	QJsonObject& pendingFields()
	{
		thread_local QJsonObject fields;
		return fields;
	}
}

QString levelName(QtMsgType type)
{
	switch (type) {
		case QtDebugMsg:
			return QStringLiteral("debug");
		case QtInfoMsg:
			return QStringLiteral("info");
		case QtWarningMsg:
			return QStringLiteral("warning");
		case QtCriticalMsg:
			return QStringLiteral("critical");
		case QtFatalMsg:
			return QStringLiteral("fatal");
	}
	return QStringLiteral("debug");
}

void log(const QLoggingCategory& category, QtMsgType type, const QString& message, const QJsonObject& fields,
	const QMessageLogContext& context)
{
	if (!category.isEnabled(type)) {
		return;
	}
	pendingFields() = fields;
	QMessageLogger logger(context.file, context.line, context.function, category.categoryName());
	switch (type) {
		case QtDebugMsg:
			logger.debug(category).noquote().nospace() << message;
			break;
		case QtInfoMsg:
			logger.info(category).noquote().nospace() << message;
			break;
		case QtWarningMsg:
			logger.warning(category).noquote().nospace() << message;
			break;
		case QtCriticalMsg:
		case QtFatalMsg: // never aborts the app from here
			logger.critical(category).noquote().nospace() << message;
			break;
	}
	// Not picked up if no message handler of ours is installed
	pendingFields() = QJsonObject();
}

QJsonObject Detail::takePendingFields() { return std::exchange(pendingFields(), QJsonObject()); }
}
