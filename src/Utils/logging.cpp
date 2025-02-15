/*
 * Copyright (c) 2025.
 */

//
// Created by pascalb on 15.02.25.
//

#include "logging.h"

namespace QRedmineClient::Utils::Logging {
void Debug(const QLoggingCategory& loggingCategory, const QString& message)
{
	qCDebug(loggingCategory) << QT_TR_NOOP(message, "");
}

void Info(const QLoggingCategory& loggingCategory, const QString& message)
{
	qCInfo(loggingCategory) << QT_TR_NOOP(message, "");
}

void Warning(const QLoggingCategory& loggingCategory, const QString& message)
{
	qCWarning(loggingCategory) << QT_TR_NOOP(message, "");
}

void Error(const QLoggingCategory& loggingCategory, const QString& message)
{
	qCCritical(loggingCategory) << QT_TR_NOOP(message, "");
}

void Fatal(const QLoggingCategory& loggingCategory, const QString& message)
{
	qCFatal(loggingCategory) << QT_TR_NOOP(message, "");
}
}
