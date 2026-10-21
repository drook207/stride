#include "logmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSysInfo>
#include <QThread>
#include <cstdio>

#include "appdatapath.h"
#include "qsettingshelper.h"

Q_LOGGING_CATEGORY(lcApp, "qredmine.app")

namespace QRedmineClient::Utils::Logging {

const QString LogManager::FILE_BASE_NAME = QStringLiteral("stride");
const QString LogManager::LEVEL_SETTING = QStringLiteral("logging/level");

namespace {
	//! Guards the log file, the buffer and the instance pointer against the threads that log
	QMutex sMutex;
	//! How often buffered output is written to the log file, apart from warnings and above (written at once)
	constexpr int FLUSH_INTERVAL_MS = 1500;
	//! Copying the log files for an export in pieces of this size
	constexpr qint64 EXPORT_CHUNK_SIZE = 1024 * 1024;

	QString jsonString(const QString& text)
	{
		// A JSON array with the one string, without its brackets
		const QByteArray array = QJsonDocument(QJsonArray { text }).toJson(QJsonDocument::Compact);
		return QString::fromUtf8(array.mid(1, array.size() - 2));
	}
}

LogManager::LogManager(const QString& directory, QObject* parent)
	: QObject(parent)
	, mDirectory(directory.isEmpty() ? appDataPath() + QStringLiteral("/logs") : directory)
{
	Q_ASSERT(!sInstance);
	qRegisterMetaType<LogEntry>();
	mLevel = levelFromName(GetSetting(LEVEL_SETTING).toString());

	// Started by record() when something is waiting to be written, so an idle application doesn't wake up
	mFlushTimer.setInterval(FLUSH_INTERVAL_MS);
	mFlushTimer.setSingleShot(true);
	connect(&mFlushTimer, &QTimer::timeout, this, &LogManager::flush);

	{
		QMutexLocker locker(&sMutex);
		openLogFile();
		sInstance = this;
		mPreviousHandler = qInstallMessageHandler(&LogManager::handleMessage);
	}
	applyLevel();
}

LogManager::~LogManager()
{
	// Back to what was there, before the manager is gone
	qInstallMessageHandler(mPreviousHandler);
	QMutexLocker locker(&sMutex);
	sInstance = nullptr;
	flushLocked();
	mFile.close();
}

LogManager* LogManager::instance() { return sInstance; }

LogManager::Level LogManager::level() const { return mLevel; }

void LogManager::setLevel(Level level)
{
	if (mLevel == level) {
		return;
	}
	mLevel = level;
	SaveSetting(LEVEL_SETTING, levelName(level));
	applyLevel();
	QR_INFO(lcApp, QStringLiteral("Log level changed to %1").arg(levelName(level)), { { "level", levelName(level) } });
	emit levelChanged();
}

QString LogManager::levelName(Level level)
{
	switch (level) {
		case Normal:
			return QStringLiteral("normal");
		case Verbose:
			return QStringLiteral("verbose");
		case Debug:
			return QStringLiteral("debug");
	}
	return QStringLiteral("normal");
}

LogManager::Level LogManager::levelFromName(const QString& name)
{
	for (const Level level : { Normal, Verbose, Debug }) {
		if (levelName(level) == name) {
			return level;
		}
	}
	return Normal;
}

QString LogManager::levelRules(Level level)
{
	// Later rules win. Only the app's own categories ("qredmine.*") tell about the app's behavior; of what
	// Qt and others log, only warnings and above are of interest.
	QStringList rules { QStringLiteral("*.debug=false"), QStringLiteral("*.info=false"),
		QStringLiteral("qredmine.*.info=true") };
	if (level == Verbose) {
		rules << QStringLiteral("qredmine.client.network.debug=true")
			  << QStringLiteral("qredmine.timetracker*.debug=true");
	} else if (level == Debug) {
		rules << QStringLiteral("qredmine.*.debug=true") << QStringLiteral("qml.debug=true")
			  << QStringLiteral("js.debug=true");
	}
	return rules.join(QLatin1Char('\n'));
}

QString LogManager::logDirectory() const { return mDirectory; }

QString LogManager::filePath(int index) const
{
	const QString name = index == 0 ? FILE_BASE_NAME + QStringLiteral(".log")
									: QStringLiteral("%1.%2.log").arg(FILE_BASE_NAME).arg(index);
	return QDir(mDirectory).filePath(name);
}

QStringList LogManager::logFiles() const
{
	QStringList files;
	for (int index = MAX_FILES - 1; index >= 0; --index) {
		if (QFileInfo::exists(filePath(index))) {
			files << filePath(index);
		}
	}
	return files;
}

QList<LogEntry> LogManager::bufferedEntries() const
{
	QMutexLocker locker(&sMutex);
	return QList<LogEntry>(mBuffer.cbegin(), mBuffer.cend());
}

bool LogManager::exportLogs(const QString& fileName)
{
	flush();
	const QFileInfo target(fileName);
	const QStringList files = logFiles();
	for (const QString& file : files) {
		if (QFileInfo(file) == target) {
			return false;
		}
	}

	QSaveFile out(fileName);
	// For targets that aren't plain files (content:// on Android), which it can't replace atomically
	out.setDirectWriteFallback(true);
	if (!out.open(QIODevice::WriteOnly)) {
		return false;
	}
	for (const QString& path : files) {
		QFile in(path);
		if (!in.open(QIODevice::ReadOnly)) {
			continue; // rotated away in the meantime
		}
		while (!in.atEnd()) {
			if (out.write(in.read(EXPORT_CHUNK_SIZE)) < 0) {
				out.cancelWriting();
				return false;
			}
		}
	}
	return out.commit();
}

void LogManager::flush()
{
	QMutexLocker locker(&sMutex);
	flushLocked();
}

void LogManager::flushLocked()
{
	mFlushScheduled = false;
	if (mDirty) {
		mFile.flush();
		mDirty = false;
	}
}

void LogManager::logStartup()
{
	const QString name = QCoreApplication::applicationName();
	const QString version = QCoreApplication::applicationVersion();
	QR_INFO(lcApp, QStringLiteral("%1 %2 started").arg(name, version),
		{ { "app", name }, { "version", version }, { "qt", QString::fromLatin1(qVersion()) },
			{ "os", QSysInfo::prettyProductName() },
			{ "kernel", QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion() },
			{ "arch", QSysInfo::currentCpuArchitecture() }, { "locale", QLocale::system().name() },
			{ "pid", QCoreApplication::applicationPid() } });
	QR_INFO(lcApp, QStringLiteral("Logging at level %1 to %2").arg(levelName(mLevel), mDirectory),
		{ { "level", levelName(mLevel) }, { "directory", mDirectory } });
}

void LogManager::logShutdown()
{
	QR_INFO(lcApp, QStringLiteral("Application is shutting down"));
	flush();
}

void LogManager::applyLevel()
{
	QString rules = levelRules(mLevel);
	// The rules of a developer's environment come last, so they win; QT_LOGGING_RULES separates them by ';'
	QString environmentRules = qEnvironmentVariable("QT_LOGGING_RULES");
	if (!environmentRules.isEmpty()) {
		rules += QLatin1Char('\n') + environmentRules.replace(QLatin1Char(';'), QLatin1Char('\n'));
	}
	QLoggingCategory::setFilterRules(rules);
}

void LogManager::handleMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
	// A message made while one is handled (e.g. by Qt itself) is only printed: recording it could recurse or
	// wait for the lock held by this very thread
	thread_local bool handling = false;
	QtMessageHandler previous = nullptr;
	if (handling) {
		printMessage(nullptr, type, context, message);
		return;
	}
	handling = true;

	LogEntry entry;
	entry.timestamp = QDateTime::currentDateTime();
	entry.timestamp = entry.timestamp.toOffsetFromUtc(entry.timestamp.offsetFromUtc());
	entry.type = type;
	entry.category = QString::fromLatin1(context.category ? context.category : "default");
	entry.message = message;
	entry.fields = Detail::takePendingFields();
	if ((type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) && context.file) {
		entry.file = QString::fromUtf8(context.file);
		entry.line = context.line;
	}
	{
		QMutexLocker locker(&sMutex);
		if (LogManager* manager = sInstance; manager) {
			previous = manager->mPreviousHandler;
			manager->record(entry);
		}
	}
	printMessage(previous, type, context, message);
	handling = false;
}

void LogManager::printMessage(
	QtMessageHandler previous, QtMsgType type, const QMessageLogContext& context, const QString& message)
{
	if (previous) {
		previous(type, context, message);
		return;
	}
	// Qt's own handler can't be called from here; this is what it prints, apart from platform specifics
	std::fprintf(stderr, "%s\n", qPrintable(qFormatLogMessage(type, context, message)));
	std::fflush(stderr);
}

void LogManager::record(const LogEntry& recorded)
{
	LogEntry entry = recorded;
	entry.sequence = mNextSequence++;

	const QByteArray line = toJsonLine(entry);
	if (mFile.isOpen()) {
		if (mFileSize > 0 && mFileSize + line.size() > MAX_FILE_SIZE) {
			rotate();
			openLogFile();
		}
		// A failure to write is ignored: logging must never get in the way of the app
		if (mFile.isOpen() && mFile.write(line) == line.size()) {
			mFileSize += line.size();
			mDirty = true;
			if (!mFlushScheduled) {
				mFlushScheduled = true;
				// The timer belongs to the manager's thread, which isn't necessarily the logging one
				QMetaObject::invokeMethod(
					&mFlushTimer, [this]() { mFlushTimer.start(); }, Qt::QueuedConnection);
			}
			if (entry.type != QtDebugMsg && entry.type != QtInfoMsg) {
				flushLocked();
			}
		}
	}

	mBuffer.push_back(entry);
	while (mBuffer.size() > static_cast<size_t>(MAX_BUFFERED_ENTRIES)) {
		mBuffer.pop_front();
	}

	// Announced from the manager's thread whichever thread logged; only posts an event, so nothing of the
	// receivers runs under the lock
	QMetaObject::invokeMethod(
		this, [this, entry]() { emit entryLogged(entry); }, Qt::QueuedConnection);
}

void LogManager::openLogFile()
{
	QDir().mkpath(mDirectory);
	mFile.setFileName(filePath(0));
	mFileSize = QFileInfo(mFile).size();
	if (mFileSize >= MAX_FILE_SIZE) {
		rotate();
		mFile.setFileName(filePath(0));
		mFileSize = 0;
	}
	// Buffered, see flush()
	if (!mFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
		mFileSize = 0;
	}
}

void LogManager::rotate()
{
	flushLocked();
	mFile.close();
	QFile::remove(filePath(MAX_FILES - 1));
	for (int index = MAX_FILES - 2; index >= 0; --index) {
		QFile::rename(filePath(index), filePath(index + 1));
	}
	mFileSize = 0;
}

QString LogManager::fieldsToJson(const QJsonObject& fields)
{
	return fields.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact));
}

QByteArray LogManager::toJsonLine(const LogEntry& entry) { return toJsonLine(entry, fieldsToJson(entry.fields)); }

QByteArray LogManager::toJsonLine(const LogEntry& entry, const QString& fieldsJson)
{
	// Assembled by hand rather than via QJsonObject, which would sort the keys; "ts" first reads best
	QString line =
		QStringLiteral(R"({"ts":%1,"level":%2,"cat":%3,"msg":%4)")
			.arg(jsonString(entry.timestamp.toString(Qt::ISODateWithMs)), jsonString(Logging::levelName(entry.type)),
				jsonString(entry.category), jsonString(entry.message));
	if (!fieldsJson.isEmpty()) {
		line += QStringLiteral(R"(,"fields":)") + fieldsJson;
	}
	if (!entry.file.isEmpty()) {
		line += QStringLiteral(R"(,"file":%1,"line":%2)").arg(jsonString(entry.file)).arg(entry.line);
	}
	line += QStringLiteral("}\n");
	return line.toUtf8();
}
}
