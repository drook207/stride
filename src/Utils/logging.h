#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include "Utils_global.h"

namespace QRedmineClient::Utils::Logging {

/*!
 * \brief The LogEntry struct is one message as the LogManager recorded it
 */
struct LogEntry {
	//! Running number of the message since startup, so a consumer can tell which ones it has seen
	quint64 sequence = 0;
	//! Local time with its offset from UTC
	QDateTime timestamp;
	QtMsgType type = QtDebugMsg;
	//! Name of the logging category, "default" for messages made without one
	QString category;
	QString message;
	//! Structured data attached with log(), empty if none
	QJsonObject fields;
	//! Where the message was made; only known for warnings and above (and only in builds that keep it)
	QString file;
	int line = 0;
};

/*!
 * \brief levelName The name a message type is written with: "debug", "info", "warning", "critical" or "fatal"
 */
UTILS_EXPORT QString levelName(QtMsgType type);

/*!
 * \brief log Logs \a message in \a category like qCDebug() and its siblings do (so the category rules and the
 * console output apply), and attaches \a fields to the entry written to the log file and shown in the app.
 *
 * Prefer the QR_LOG macros, which also pass where the message was made and don't evaluate their arguments for
 * a message that is filtered out.
 * \param fields Structured data, e.g. {{"status", 200}}. Never put secrets (the API key) in it.
 * \param context Where the message was made
 */
UTILS_EXPORT void log(const QLoggingCategory& category, QtMsgType type, const QString& message,
	const QJsonObject& fields = QJsonObject(), const QMessageLogContext& context = QMessageLogContext());

namespace Detail {
	//! Takes (and clears) the fields attached to the message the calling thread is logging right now, for the
	//! LogManager's message handler
	UTILS_EXPORT QJsonObject takePendingFields();
}
}

Q_DECLARE_METATYPE(QRedmineClient::Utils::Logging::LogEntry)

//! Logs a message with structured fields, e.g.
//! QR_LOG(lcFoo, QtInfoMsg, QStringLiteral("Saved %1").arg(id), { { "id", id }, { "ok", true } }).
//! The message and fields are only evaluated if the category is enabled for the type. The fields are optional.
#define QR_LOG(category, type, message, ...)                                                                           \
	do {                                                                                                               \
		if ((category)().isEnabled(type)) {                                                                            \
			::QRedmineClient::Utils::Logging::log((category)(), type, message, QJsonObject(__VA_ARGS__),               \
				QMessageLogContext(__FILE__, __LINE__, Q_FUNC_INFO, (category)().categoryName()));                     \
		}                                                                                                              \
	} while (false)

#define QR_DEBUG(category, message, ...) QR_LOG(category, QtDebugMsg, message, __VA_ARGS__)
#define QR_INFO(category, message, ...) QR_LOG(category, QtInfoMsg, message, __VA_ARGS__)
#define QR_WARNING(category, message, ...) QR_LOG(category, QtWarningMsg, message, __VA_ARGS__)
