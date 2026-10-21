#include "logmodel.h"

#include <QMetaType>

#include "logmanager.h"

namespace Stride {

using QRedmineClient::Utils::Logging::LogEntry;
using QRedmineClient::Utils::Logging::LogManager;

LogEntryModel::LogEntryModel(QObject* parent)
	: QAbstractListModel(parent)
{
	LogManager* manager = LogManager::instance();
	if (!manager) {
		return;
	}
	// Connected before the buffer is read, so no message is missed in between; the ones in both are skipped
	connect(manager, &LogManager::entryLogged, this, &LogEntryModel::append);
	for (const LogEntry& entry : manager->bufferedEntries()) {
		append(entry);
	}
}

int LogEntryModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(mEntries.size());
}

QVariant LogEntryModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() >= mEntries.size()) {
		return {};
	}
	const LogEntry& entry = mEntries.at(index.row()).entry;
	switch (role) {
		case TimestampRole:
			return entry.timestamp;
		case LevelRole:
			return QRedmineClient::Utils::Logging::levelName(entry.type);
		case LevelValueRole:
			return severity(entry.type);
		case CategoryRole:
			return entry.category;
		case MessageRole:
			return entry.message;
		case FieldsRole:
			return mEntries.at(index.row()).fieldsJson;
		case SourceRole:
			return entry.file.isEmpty() ? QString() : QStringLiteral("%1:%2").arg(entry.file).arg(entry.line);
		case SequenceRole:
			return entry.sequence;
		default:
			return {};
	}
}

QHash<int, QByteArray> LogEntryModel::roleNames() const
{
	return { { TimestampRole, "ts" }, { LevelRole, "level" }, { LevelValueRole, "levelValue" },
		{ CategoryRole, "category" }, { MessageRole, "message" }, { FieldsRole, "fields" }, { SourceRole, "source" },
		{ SequenceRole, "sequence" } };
}

const LogEntry& LogEntryModel::entryAt(int row) const { return mEntries.at(row).entry; }

const QString& LogEntryModel::fieldsJsonAt(int row) const { return mEntries.at(row).fieldsJson; }

QStringList LogEntryModel::categories() const { return mCategories; }

int LogEntryModel::severity(QtMsgType type)
{
	switch (type) {
		case QtDebugMsg:
			return 0;
		case QtInfoMsg:
			return 1;
		case QtWarningMsg:
			return 2;
		case QtCriticalMsg:
			return 3;
		case QtFatalMsg:
			return 4;
	}
	return 0;
}

void LogEntryModel::append(const LogEntry& entry)
{
	if (entry.sequence <= mLastSequence) {
		return;
	}
	mLastSequence = entry.sequence;

	beginInsertRows(QModelIndex(), static_cast<int>(mEntries.size()), static_cast<int>(mEntries.size()));
	mEntries.append({ entry, LogManager::fieldsToJson(entry.fields) });
	endInsertRows();

	// Dropped in steps rather than one by one, so a busy log doesn't keep the views busy
	const qsizetype excess = mEntries.size() - LogManager::MAX_BUFFERED_ENTRIES;
	if (excess >= LogManager::MAX_BUFFERED_ENTRIES / 10) {
		beginRemoveRows(QModelIndex(), 0, static_cast<int>(excess) - 1);
		mEntries.remove(0, excess);
		endRemoveRows();
	}

	if (const auto it = std::lower_bound(mCategories.cbegin(), mCategories.cend(), entry.category);
		it == mCategories.cend() || *it != entry.category) {
		mCategories.insert(it - mCategories.cbegin(), entry.category);
		emit categoriesChanged();
	}
}

LogModel::LogModel(QObject* parent)
	: QSortFilterProxyModel(parent)
	, mSource(new LogEntryModel(this))
{
	setSourceModel(mSource);
	connect(mSource, &LogEntryModel::categoriesChanged, this, &LogModel::categoriesChanged);
	connect(this, &QAbstractItemModel::rowsInserted, this, &LogModel::countChanged);
	connect(this, &QAbstractItemModel::rowsRemoved, this, &LogModel::countChanged);
	connect(this, &QAbstractItemModel::modelReset, this, &LogModel::countChanged);
	connect(this, &QAbstractItemModel::layoutChanged, this, &LogModel::countChanged);
}

LogModel::Level LogModel::minimumLevel() const { return mMinimumLevel; }

void LogModel::setMinimumLevel(Level level)
{
	if (mMinimumLevel == level) {
		return;
	}
	mMinimumLevel = level;
	refilter();
	emit minimumLevelChanged();
}

QString LogModel::category() const { return mCategory; }

void LogModel::setCategory(const QString& category)
{
	if (mCategory == category) {
		return;
	}
	mCategory = category;
	refilter();
	emit categoryChanged();
}

QString LogModel::searchText() const { return mSearchText; }

void LogModel::setSearchText(const QString& text)
{
	if (mSearchText == text) {
		return;
	}
	mSearchText = text;
	refilter();
	emit searchTextChanged();
}

void LogModel::refilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
	beginFilterChange();
	endFilterChange(Direction::Rows);
#else
	invalidateFilter();
#endif
}

QStringList LogModel::categories() const { return mSource->categories(); }

QString LogModel::rowAsJson(int row) const
{
	const QModelIndex source = mapToSource(index(row, 0));
	if (!source.isValid()) {
		return {};
	}
	return QString::fromUtf8(
		LogManager::toJsonLine(mSource->entryAt(source.row()), mSource->fieldsJsonAt(source.row())))
		.trimmed();
}

bool LogModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	Q_UNUSED(sourceParent)
	const LogEntry& entry = mSource->entryAt(sourceRow);
	if (LogEntryModel::severity(entry.type) < static_cast<int>(mMinimumLevel)) {
		return false;
	}
	if (!mCategory.isEmpty() && entry.category != mCategory) {
		return false;
	}
	if (mSearchText.isEmpty()) {
		return true;
	}
	return entry.message.contains(mSearchText, Qt::CaseInsensitive) ||
		entry.category.contains(mSearchText, Qt::CaseInsensitive) ||
		mSource->fieldsJsonAt(sourceRow).contains(mSearchText, Qt::CaseInsensitive);
}
}
