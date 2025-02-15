/*
 * Copyright (c) 2025.
 */

//
// Created by pascalb on 15.02.25.
//

#pragma once

#include <QLoggingCategory>

namespace QRedmineClient::Utils::Logging {
static void Debug(const QLoggingCategory& loggingCategory, const QString& message);

static void Info(const QLoggingCategory& loggingCategory, const QString& message);

static void Warning(const QLoggingCategory& loggingCategory, const QString& message);

static void Error(const QLoggingCategory& loggingCategory, const QString& message);
static void Fatal(const QLoggingCategory& loggingCategory, const QString& message);
}
