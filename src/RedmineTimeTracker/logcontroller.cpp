#include "logcontroller.h"

#include <QDateTime>

#include "logmanager.h"

namespace QRedmineClient {

using Utils::Logging::LogManager;

LogController::LogController(QObject* parent)
	: QObject(parent)
{
	if (LogManager* manager = LogManager::instance()) {
		connect(manager, &LogManager::levelChanged, this, &LogController::levelChanged);
	}
}

QString LogController::level() const
{
	LogManager* manager = LogManager::instance();
	return LogManager::levelName(manager ? manager->level() : LogManager::Normal);
}

void LogController::setLevel(const QString& level)
{
	if (LogManager* manager = LogManager::instance()) {
		manager->setLevel(LogManager::levelFromName(level));
	}
}

QStringList LogController::availableLevels() const
{
	return { LogManager::levelName(LogManager::Normal), LogManager::levelName(LogManager::Verbose),
		LogManager::levelName(LogManager::Debug) };
}

QUrl LogController::logDirectory() const
{
	LogManager* manager = LogManager::instance();
	return manager ? QUrl::fromLocalFile(manager->logDirectory()) : QUrl();
}

bool LogController::exportLogs(const QUrl& file)
{
	LogManager* manager = LogManager::instance();
	return manager && manager->exportLogs(file.isLocalFile() ? file.toLocalFile() : file.toString());
}

QString LogController::exportFileName() const
{
	return QStringLiteral("stride-logs-%1.jsonl")
		.arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
}
}
