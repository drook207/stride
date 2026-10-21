#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace Stride {
/*!
 * \brief The LogController class is what the UI uses of the application's logging: the detail level, the log
 * files and the in-app log view
 *
 * Exposed to QML as a singleton. Works on the Utils::Logging::LogManager created in main.cpp; without one
 * (as in tests) the level is "normal" and nothing is exported.
 */
class LogController : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON
	//! The detail level, one of availableLevels; persisted and applied at once. "normal": information,
	//! warnings and errors; "verbose": also every request to Redmine in detail and time tracking details;
	//! "debug": everything, including what the UI logs.
	Q_PROPERTY(QString level READ level WRITE setLevel NOTIFY levelChanged FINAL)
	Q_PROPERTY(QStringList availableLevels READ availableLevels CONSTANT FINAL)
	//! The directory of the log files, e.g. for Qt.openUrlExternally()
	Q_PROPERTY(QUrl logDirectory READ logDirectory CONSTANT FINAL)
public:
	explicit LogController(QObject* parent = nullptr);

	QString level() const;
	void setLevel(const QString& level);
	QStringList availableLevels() const;
	QUrl logDirectory() const;

	/*!
	 * \brief exportLogs Writes all log files, the oldest messages first, into one file (JSON Lines)
	 * \param file File to create, e.g. from a file dialog (a local file, or a content:// URL on Android);
	 * replaced if it exists
	 * \return Whether it was written completely
	 */
	Q_INVOKABLE bool exportLogs(const QUrl& file);

	/*!
	 * \brief exportFileName Suggested name of the exported file, with the current time
	 * \return e.g. "stride-logs-20261020-143211.jsonl"
	 */
	Q_INVOKABLE QString exportFileName() const;

signals:
	void levelChanged();
};
}
