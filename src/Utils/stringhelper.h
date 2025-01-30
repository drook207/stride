#pragma once

#include <QString>
#include <QStringLiteral>

#define STATIC_CONST_QSTRING(name, string) static const QString name = QStringLiteral(string)
