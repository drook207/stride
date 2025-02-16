/*
 * Copyright (c) 2025.
 */

//
// Created by pascalb on 15.02.25.
//

#pragma once

#include <QLoggingCategory>

namespace QRedmineClient::Utils::Logging {
template <typename... Args>
static void Debug(const QLoggingCategory& loggingCategory, const QString& message, const Args&... args)
{
	qCDebug(loggingCategory) << QT_TR_NOOP(message.arg(args...));
}

template <typename... Args>
static void Info(const QLoggingCategory& loggingCategory, const QString& message, const Args&... args)
{
	qCInfo(loggingCategory) << QT_TR_NOOP(message.arg(args...));
}

template <typename... Args>
static void Warning(const QLoggingCategory& loggingCategory, const QString& message, const Args&... args)
{
	qCWarning(loggingCategory) << QT_TR_NOOP(message.arg(args...));
}

template <typename... Args>
static void Error(const QLoggingCategory& loggingCategory, const QString& message, const Args&... args)
{
	qCCritical(loggingCategory) << QT_TR_NOOP(message.arg(args...));
}

template <typename... Args>
static void Fatal(const QLoggingCategory& loggingCategory, const QString& message, const Args&... args)
{
	qCFatal(loggingCategory) << QT_TR_NOOP(message.arg(args...));
}
}
