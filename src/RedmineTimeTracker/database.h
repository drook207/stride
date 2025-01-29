#include <QJsonArray>
#include <QString>
#include <QStringLiteral>

namespace QRedmineClient {
class Database {
public:
	static bool initialize();

	static bool addTimeEntry(
		int issueId, double hours, const QString& comments, const QDateTime& spentOn = QDateTime::currentDateTime());

	static QVector<QVariantMap> getUnsyncedEntries();

	static QJsonArray getRecentEntries(int limit = 50);

public slots:
	static bool updateSyncStatus(int id, int redmineId);
	static bool addTimeEntryActivities(const QJsonArray& activities);
};
}
