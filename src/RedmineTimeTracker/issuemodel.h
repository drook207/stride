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
	//! Counts reloads of the issue list. hasIssue() and projectIdOf() are functions, which QML can't
	//! tell have changed: bindings calling them also read revision, so they're evaluated again.
	Q_PROPERTY(int revision READ revision NOTIFY revisionChanged FINAL)
public:
	enum IssueRoles {
		IdRole = Qt::UserRole + 1,
		IdAndSubjectRole,
		SubjectRole,
		ProjectIdRole,
		ProjectNameRole,
		StatusIdRole,
		DescriptionRole,
		CreatedOnRole,
		UpdatedOnRole
	};

	Q_ENUM(IssueRoles);

	IssueModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	Q_INVOKABLE bool hasIssue(int issue);

	/*!
	 * \brief projectIdOf Project of a cached issue
	 * \return The project id, or -1 if the issue isn't cached
	 */
	Q_INVOKABLE int projectIdOf(int issueId) const;

	/*!
	 * \brief idAndSubjectOf A cached issue as the issue field shows it ("<id>: <subject>")
	 * \return The text, or just the id if the issue isn't cached
	 */
	Q_INVOKABLE QString idAndSubjectOf(int issueId) const;

	int revision() const;

signals:
	void revisionChanged();

public slots:
	void updateIssueList();

protected:
	QHash<int, QByteArray> roleNames() const override;

private:
	struct Issue {
		int id;
		QString subject;
		int projectId;
		QString projectName;
		int statusId;
		QString description;
		QDateTime createdOn;
		QDateTime updatedOn;
	};

	int mRevision = 0;

	QList<Issue> mIssues;
};

} // namespace QRedmineClient
