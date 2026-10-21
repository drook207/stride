#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include "logging.h"

namespace Stride {
/*!
 * \brief The LogEntryModel class lists the messages the Utils::Logging::LogManager recorded: the buffered ones
 * at creation, then every new one, at most LogManager::MAX_BUFFERED_ENTRIES (the oldest are dropped). Oldest first.
 *
 * The data of LogModel; not used directly.
 */
class LogEntryModel : public QAbstractListModel {
	Q_OBJECT
public:
	enum Roles {
		TimestampRole = Qt::UserRole + 1,
		LevelRole,
		LevelValueRole,
		CategoryRole,
		MessageRole,
		FieldsRole,
		SourceRole,
		//! The entry's running number, unique for the app's run (e.g. to tell entries apart in a view)
		SequenceRole
	};

	explicit LogEntryModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	//! The entry of a row
	const QRedmineClient::Utils::Logging::LogEntry& entryAt(int row) const;

	//! The fields of the entry of a row as compact JSON, empty if none; made once per entry
	const QString& fieldsJsonAt(int row) const;

	//! Every category of an entry so far, sorted
	QStringList categories() const;

	//! How severe a message type is: 0 (debug), 1 (info), 2 (warning), 3 (critical), 4 (fatal)
	static int severity(QtMsgType type);

signals:
	void categoriesChanged();

private slots:
	//! Adds a message, unless it is in the model already
	void append(const QRedmineClient::Utils::Logging::LogEntry& entry);

private:
	//! An entry with what is made of it once, as it's needed again for every view update and filtering
	struct Row {
		QRedmineClient::Utils::Logging::LogEntry entry;
		QString fieldsJson;
	};

	QList<Row> mEntries;
	QStringList mCategories;
	//! Sequence number of the latest entry added, to skip entries received twice
	quint64 mLastSequence = 0;
};

/*!
 * \brief The LogModel class is the application's log, for a list view: the messages recorded by the
 * Utils::Logging::LogManager (the latest 5000, oldest first), filtered by severity, category and a search text
 *
 * New messages show up at the end of the list as they are logged. Without a LogManager (as in tests) the model
 * stays empty.
 *
 * Roles: ts (QDateTime), level ("debug", "info", "warning", "critical", "fatal"), levelValue (0 to 4, see
 * minimumLevel), category, message, fields (the structured data as compact JSON, empty if none) and source
 * ("file:line", empty unless known).
 */
class LogModel : public QSortFilterProxyModel {
	Q_OBJECT
	QML_ELEMENT
	//! Only messages at least this severe are listed, a value of Level
	Q_PROPERTY(Level minimumLevel READ minimumLevel WRITE setMinimumLevel NOTIFY minimumLevelChanged FINAL)
	//! Only messages of this category are listed; every category if empty
	Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged FINAL)
	//! Only messages containing this text (case-insensitive) in their message, category or fields are listed
	Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged FINAL)
	//! The categories seen so far, sorted, for choosing one
	Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged FINAL)
	//! Number of messages listed
	Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
public:
	enum Level { Debug, Info, Warning, Critical, Fatal };
	Q_ENUM(Level)

	explicit LogModel(QObject* parent = nullptr);

	Level minimumLevel() const;
	void setMinimumLevel(Level level);
	QString category() const;
	void setCategory(const QString& category);
	QString searchText() const;
	void setSearchText(const QString& text);
	QStringList categories() const;

	/*!
	 * \brief rowAsJson A listed message as it is written to the log file (one JSON object), e.g. to copy it
	 * \param row Row of this model
	 * \return Empty if there is no such row
	 */
	Q_INVOKABLE QString rowAsJson(int row) const;

signals:
	void minimumLevelChanged();
	void categoryChanged();
	void searchTextChanged();
	void categoriesChanged();
	void countChanged();

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
	//! Applies the changed filter to the rows
	void refilter();

	LogEntryModel* mSource;
	Level mMinimumLevel = Debug;
	QString mCategory;
	QString mSearchText;
};
}
