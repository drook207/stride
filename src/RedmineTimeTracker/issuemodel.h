#pragma once

#include "database.h"
#include <QAbstractListModel>
#include <QDateTime>
#include <QtQmlIntegration>

namespace QRedmineClient {

class IssueModel : public QAbstractListModel {
	Q_OBJECT
	QML_ELEMENT

	Q_DISABLE_COPY_MOVE(IssueModel)
public:
	enum IssueRoles {
		IdRole = Qt::UserRole + 1,
		IdAndSubjectRole,
		SubjectRole,
		ProjectIdRole,
		StatusIdRole,
		DescriptionRole,
		CreatedOnRole,
		UpdatedOnRole
	};

	IssueModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
public slots:
	void updateIssueList();

protected:
	QHash<int, QByteArray> roleNames() const override;

private:
	struct Issue {
		int id;
		QString subject;
		int projectId;
		int statusId;
		QString description;
		QDateTime createdOn;
		QDateTime updatedOn;
	};

	QList<Issue> mIssues;
};

} // namespace QRedmineClient
