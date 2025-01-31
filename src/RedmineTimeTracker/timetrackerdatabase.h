#include <QDateTime>
#include <QJsonArray>
#include <QString>
#include <QStringLiteral>

namespace QRedmineClient {
class TimeEntryDatabase {
public:
	static bool initialize();

	static bool addTimeEntry(int issueId, double hours, const QString& comments);

	static QVector<QVariantMap> getUnsyncedEntries();

	static QJsonArray getRecentEntries(int limit = 50);

	static bool createIssuesDatabase();

	static bool addIssue(int issueId, int projectId, int statusId, const QString& description,
		const QDateTime& createdOn, const QDateTime& updatedOn);
public slots:
	static bool updateSyncStatus(int id, int redmineId);
};
}
