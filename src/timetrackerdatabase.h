#include <QString>
#include <QStringLiteral>

namespace QRedmineClient {
class TimeEntryDatabase {
public:
	static bool initialize();

	static bool addTimeEntry(int issueId, double hours, const QString& comments);

	static bool updateSyncStatus(int id, int redmineId);

	static QVector<QVariantMap> getUnsyncedEntries();

	static QVector<QVariantMap> getRecentEntries(int limit = 50);
};
}
