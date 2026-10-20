#pragma once

#include <QDateTime>
#include <QFile>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <deque>

#include "Utils_global.h"
#include "logging.h"

namespace QRedmineClient::Utils::Logging {
/*!
 * \brief The LogManager class records everything the application logs, to analyze its behavior (in particular
 * its traffic to Redmine) on a user's machine
 *
 * Installs a Qt message handler, so every message of every thread and category (including QML's console.*) is
 *  - passed on to the previously installed handler, so the console output stays as it is,
 *  - appended to a log file as one JSON object per line (JSON Lines), see logDirectory(),
 *  - kept in memory (the latest MAX_BUFFERED_ENTRIES) for the in-app log view, see bufferedEntries(),
 *  - announced via entryLogged().
 *
 * Log file: "<AppLocalDataLocation>/logs/stride.log", e.g.
 * {"ts":"2026-10-20T14:32:11.123+02:00","level":"info","cat":"qredmine.client.network","msg":"GET issues.json
 * finished","fields":{"id":12,"status":200,"durationMs":143}}
 * "file" and "line" are added for warnings and above. At MAX_FILE_SIZE the file is renamed to "stride.1.log", an
 * older "stride.1.log" to "stride.2.log" and so on; at most MAX_FILES files are kept.
 *
 * The detail level (see Level) is persisted in QSettings ("logging/level") and applied via
 * QLoggingCategory::setFilterRules(). A QT_LOGGING_RULES set in the environment (e.g. by a developer) is applied
 * after the level's rules, so it wins over them.
 *
 * Create one instance in main() right after the application identity is set; it is used via instance() from then
 * on. Messages are only recorded while it exists.
 */
class UTILS_EXPORT LogManager : public QObject {
	Q_OBJECT
	Q_PROPERTY(Level level READ level WRITE setLevel NOTIFY levelChanged FINAL)
public:
	//! How much is logged. The rules per level are described by levelRules().
	enum Level {
		Normal, //!< Information, warnings and errors of the app
		Verbose, //!< Additionally the details of every request to Redmine and of time tracking
		Debug //!< Additionally everything else of the app and QML's console.log()
	};
	Q_ENUM(Level)

	//! Messages kept in memory for the in-app view
	static constexpr int MAX_BUFFERED_ENTRIES = 5000;
	//! A log file is rotated once it reaches this size, and this many files are kept (stride.log, stride.1.log, ...)
	static constexpr qint64 MAX_FILE_SIZE = 5 * 1024 * 1024;
	static constexpr int MAX_FILES = 5;
	//! Name of the current log file; the older ones are "<base>.<n>.log"
	static const QString FILE_BASE_NAME;
	//! QSettings key the level is persisted under
	static const QString LEVEL_SETTING;

	/*!
	 * \brief LogManager Opens the log file, installs the message handler and applies the persisted level
	 * \param directory Where to put the log files; the application's local data directory + "/logs" if empty
	 * \param parent
	 */
	explicit LogManager(const QString& directory = QString(), QObject* parent = nullptr);

	//! Flushes and closes the log file and restores the message handler that was installed before
	~LogManager() override;

	Q_DISABLE_COPY_MOVE(LogManager)

	/*!
	 * \brief instance The one LogManager, nullptr if none exists
	 */
	static LogManager* instance();

	/*!
	 * \brief level
	 * \return The detail level
	 */
	Level level() const;

	/*!
	 * \brief setLevel Changes the detail level at once and persists it
	 */
	void setLevel(Level level);

	/*!
	 * \brief levelName The persisted/QML name of a level: "normal", "verbose" or "debug"
	 */
	static QString levelName(Level level);

	/*!
	 * \brief levelFromName The level with a name given by levelName(); Normal for an unknown one
	 */
	static Level levelFromName(const QString& name);

	/*!
	 * \brief levelRules The QLoggingCategory filter rules (one per line) of a level
	 */
	static QString levelRules(Level level);

	/*!
	 * \brief logDirectory The directory of the log files
	 */
	QString logDirectory() const;

	/*!
	 * \brief logFiles The existing log files from the oldest to the newest one
	 * \return Absolute paths
	 */
	QStringList logFiles() const;

	/*!
	 * \brief bufferedEntries The latest messages (at most MAX_BUFFERED_ENTRIES), the oldest first
	 */
	QList<LogEntry> bufferedEntries() const;

	/*!
	 * \brief exportLogs Writes all log files, from the oldest to the newest one, into a single file
	 * \param fileName The file to create (replaced if it exists), a local path or a URL of a platform (e.g.
	 * content:// on Android)
	 * \return Whether it was written completely; not if it is one of the log files
	 */
	bool exportLogs(const QString& fileName);

	/*!
	 * \brief toJsonLine An entry as it is written to the log file: one JSON object followed by a newline
	 */
	static QByteArray toJsonLine(const LogEntry& entry);

	/*!
	 * \brief fieldsToJson The fields of an entry as compact JSON, empty if there are none
	 */
	static QString fieldsToJson(const QJsonObject& fields);

	/*!
	 * \brief toJsonLine As above, for a caller that has the entry's fields as JSON already (fieldsToJson())
	 */
	static QByteArray toJsonLine(const LogEntry& entry, const QString& fieldsJson);

	/*!
	 * \brief flush Writes what is buffered to the log file
	 */
	void flush();

	/*!
	 * \brief logStartup Logs what is needed to make sense of a log: the application and its version, Qt, the
	 * operating system, the locale, the log level and where the log is. Call once after creating the manager.
	 */
	void logStartup();

	/*!
	 * \brief logShutdown Logs that the application ends and flushes the log file
	 */
	void logShutdown();

signals:
	/*!
	 * \brief entryLogged Emitted (queued, in the manager's thread) for every message, whichever thread made it
	 */
	void entryLogged(const QRedmineClient::Utils::Logging::LogEntry& entry);

	void levelChanged();

private:
	//! The message handler installed with qInstallMessageHandler()
	static void handleMessage(QtMsgType type, const QMessageLogContext& context, const QString& message);

	//! Prints a message to the console with \a previous handler, or like Qt's default one if there is none
	static void printMessage(
		QtMessageHandler previous, QtMsgType type, const QMessageLogContext& context, const QString& message);

	//! Applies the level's rules, followed by the ones of QT_LOGGING_RULES
	void applyLevel();

	//! Writes an entry to the file and the buffer, and announces it. Called with sMutex held.
	void record(const LogEntry& entry);
	void flushLocked();
	//! Opens the current log file for appending, rotating first if it is full already
	void openLogFile();
	//! Closes the current log file and renames the files one number up, dropping the oldest
	void rotate();
	QString filePath(int index) const;

	QString mDirectory;
	Level mLevel = Normal;
	QFile mFile;
	qint64 mFileSize = 0;
	//! Whether something was written since the last flush
	bool mDirty = false;
	//! Whether mFlushTimer is started (or about to be), so it is only started once per write burst
	bool mFlushScheduled = false;
	std::deque<LogEntry> mBuffer;
	quint64 mNextSequence = 1;
	//! Single-shot, started by the first message written after a flush, so the file is up to date without
	//! writing through on every message
	QTimer mFlushTimer;
	QtMessageHandler mPreviousHandler = nullptr;

	static inline std::atomic<LogManager*> sInstance { nullptr };
};
}
