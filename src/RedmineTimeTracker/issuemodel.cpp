#include "issuemodel.h"

namespace QRedmineClient {

void IssueModel::updateIssueList()
{
	beginResetModel();
	mIssues.clear();

	QSqlQuery query = Database::getIssues();

	while (query.next()) {
		Issue issue = { query.value(0).toInt(), query.value(1).toString(), query.value(2).toInt(),
			query.value(3).toInt(), query.value(4).toString(), query.value(5).toDateTime(),
			query.value(6).toDateTime() };
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

QHash<int, QByteArray> IssueModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[IdRole] = "id";
	roles[IdAndSubjectRole] = "idAndSubject";
	roles[SubjectRole] = "subject";
	roles[ProjectIdRole] = "projectId";
	roles[StatusIdRole] = "statusId";
	roles[DescriptionRole] = "description";
	roles[CreatedOnRole] = "createdOn";
	roles[UpdatedOnRole] = "updatedOn";
	return roles;
}

} // namespace QRedmineClient
