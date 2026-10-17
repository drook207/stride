#include "issuemodel.h"

namespace QRedmineClient {

void IssueModel::updateIssueList()
{
	beginResetModel();
	mIssues.clear();

	QSqlQuery query = Database::getIssues();

	while (query.next()) {
		Issue issue = { query.value("id").toInt(), query.value("subject").toString(), query.value("project_id").toInt(),
			query.value("project_name").toString(), query.value("status_id").toInt(),
			query.value("description").toString(), query.value("created_on").toDateTime(),
			query.value("updated_on").toDateTime() };
		mIssues.append(issue);
	}

	endResetModel();
}

IssueModel::IssueModel(QObject* parent)
	: QAbstractListModel(parent)
{
	Database::initialize();
	updateIssueList();
}

int IssueModel::rowCount(const QModelIndex& parent) const { return mIssues.count(); }

QVariant IssueModel::data(const QModelIndex& index, int role) const
{
	if (index.row() < 0 || index.row() >= mIssues.count()) {
		return QVariant();
	}

	const Issue& issue = mIssues[index.row()];

	switch (static_cast<IssueRoles>(role)) {
		case IdRole:
			return issue.id;
		case IdAndSubjectRole:
			return QStringLiteral("%0: %1").arg(issue.id).arg(issue.subject);
		case SubjectRole:
			return issue.subject;
		case ProjectIdRole:
			return issue.projectId;
		case ProjectNameRole:
			return issue.projectName;
		case StatusIdRole:
			return issue.statusId;
		case DescriptionRole:
			return issue.description;
		case CreatedOnRole:
			return issue.createdOn;
		case UpdatedOnRole:
			return issue.updatedOn;
	}

	return QVariant();
}

bool IssueModel::hasIssue(int issue)
{
	const auto it = std::find_if(mIssues.begin(), mIssues.end(), [&issue](const Issue& i) { return i.id == issue; });
	return it != mIssues.end();
}

int IssueModel::projectIdOf(int issueId) const
{
	const auto it =
		std::find_if(mIssues.cbegin(), mIssues.cend(), [issueId](const Issue& issue) { return issue.id == issueId; });
	return it != mIssues.cend() ? it->projectId : -1;
}

QHash<int, QByteArray> IssueModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[IdRole] = "id";
	roles[IdAndSubjectRole] = "idAndSubject";
	roles[SubjectRole] = "subject";
	roles[ProjectIdRole] = "projectId";
	roles[ProjectNameRole] = "projectName";
	roles[StatusIdRole] = "statusId";
	roles[DescriptionRole] = "description";
	roles[CreatedOnRole] = "createdOn";
	roles[UpdatedOnRole] = "updatedOn";
	return roles;
}

} // namespace QRedmineClient
