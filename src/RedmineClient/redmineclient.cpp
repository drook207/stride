#include "redmineclient.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QPointer>
#include <QRegularExpression>
#include <QStringLiteral>
#include <QTranslator>
#include <QUrlQuery>
#include <QtLogging>

#include "qsettingshelper.h"
#include "securestorage.h"

Q_LOGGING_CATEGORY(lcRedmineClient, "qredmine.client")

namespace QRedmineClient {

// Key the API key is stored under within keychainService(), and the legacy QSettings key it used to
// be (insecurely) persisted under before it was migrated to the OS-native secure storage.
static const QString API_KEY_KEYCHAIN_KEY = QStringLiteral("apiKey");
static const QString LEGACY_API_KEY_SETTING = QStringLiteral("client/apiKey");
//! Request attribute carrying the requestTag of createTimeEntry()/updateTimeEntry() to the reply
static constexpr auto REQUEST_TAG_ATTRIBUTE = QNetworkRequest::User;
//! Request attribute marking a request whose failure isn't reported via errorOccurred()
static constexpr auto QUIET_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);
//! Request attribute carrying the getAllProjects() call (mProjectsGeneration) a page belongs to
static constexpr auto PROJECTS_GENERATION_ATTRIBUTE =
	static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2);
//! Request attribute carrying a RedmineClient::RequestTag (routing of separately handled replies)
static constexpr auto ROUTING_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 3);
//! Request attribute carrying the getTimeEntryCustomFields() call (mCustomFieldsGeneration) a reply
//! belongs to
static constexpr auto CUSTOM_FIELDS_GENERATION_ATTRIBUTE =
	static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 4);
//! Request attribute carrying the getOwnTimeEntries() call (mOwnTimeEntriesGeneration) a page belongs to
static constexpr auto OWN_TIME_ENTRIES_GENERATION_ATTRIBUTE =
	static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 5);
//! Page size used by getAllProjects() and getOwnTimeEntries(); 100 is the most Redmine returns per
//! request
static constexpr int PAGE_SIZE = 100;

// Service identifier the API key is stored under in the OS-native secure storage (see securestorage.h).
// Derived from the application name rather than a fixed literal so that, e.g., the test binary
// (which sets its own QCoreApplication::applicationName(), see tests/main.cpp) never reads from or
// writes to the same keychain entry as the real application.
static QString keychainService()
{
	const QString appName = QCoreApplication::applicationName();
	return appName.isEmpty() ? QStringLiteral("QRedmineTimeTracker") : appName;
}

// Id of the resource a request was for, from the end of its path ("…/time_entries/42.json" -> 42),
// so it works no matter how many path segments the base URL has (e.g. https://host/redmine/).
static int resourceIdFromPath(const QString& path)
{
	static const QRegularExpression idPattern(QStringLiteral("/(\\d+)\\.json$"));
	const auto match = idPattern.match(path);
	return match.hasMatch() ? match.captured(1).toInt() : -1;
}

const QList<QNetworkReply::NetworkError> RedmineClient::REASONS_FOR_CONNECTION_LOSS = {
	QNetworkReply::HostNotFoundError, QNetworkReply::TimeoutError, QNetworkReply::OperationCanceledError,
	QNetworkReply::TemporaryNetworkFailureError, QNetworkReply::AuthenticationRequiredError,
	QNetworkReply::UnknownServerError
};

bool RedmineClient::installTranslations(const QLocale& locale)
{
	// The catalog installed by the previous call, if any, so that switching the language at runtime
	// replaces it instead of stacking another translator on top of it.
	static QPointer<QTranslator> installedTranslator;
	if (installedTranslator) {
		QCoreApplication::removeTranslator(installedTranslator);
		delete installedTranslator;
	}

	// Parented to the application so it lives exactly as long as the application it is installed on.
	auto* translator = new QTranslator(QCoreApplication::instance());
	if (!translator->load(locale, QStringLiteral("QRedmineClient"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
		delete translator;
		return false;
	}
	installedTranslator = translator;
	return QCoreApplication::installTranslator(translator);
}

void QRedmineClient::RedmineClient::setupConnectionCheckTimer()
{
	mConnectionCheckTimer.setInterval(5000);
	connect(&mConnectionCheckTimer, &QTimer::timeout, this, &RedmineClient::checkConnection);

	connect(this, &RedmineClient::baseUrlChanged, this, &RedmineClient::activatePeriodicConnectionCheck);
	connect(this, &RedmineClient::apiKeyChanged, this, &RedmineClient::activatePeriodicConnectionCheck);
	connect(this, &RedmineClient::connectionStateChanged, this, [&] {
		QString state = QMetaEnum::fromType<RedmineClient::ConnectionState>().valueToKey(mConnectionState);
		qDebug(lcRedmineClient) << QStringLiteral("Connection state changed to: %0").arg(state);
	});
	activatePeriodicConnectionCheck();
}

RedmineClient::RedmineClient(QObject* parent)
	: QObject(parent)
	, mLoading(false)
	, mNetworkManager(new QNetworkAccessManager(this))
{
	connect(mNetworkManager, &QNetworkAccessManager::finished, this, &RedmineClient::handleNetworkReply);
	auto url = Utils::GetSetting("client/baseUrl").toString();
	if (!url.isEmpty()) {
		setBaseUrl(url);
	}

	// Migrate a legacy plaintext API key out of QSettings into the OS-native secure storage, if one
	// is still present from before this migration existed. setApiKey() already takes care of writing
	// the new value to the keychain and updating mApiKey/emitting apiKeyChanged synchronously.
	auto legacyKey = Utils::GetSetting(LEGACY_API_KEY_SETTING).toString();
	if (!legacyKey.isEmpty()) {
		setApiKey(legacyKey);
		Utils::RemoveSetting(LEGACY_API_KEY_SETTING);
	} else {
		// Nothing to migrate, load the API key from the keychain asynchronously instead. Applying it
		// here must not trigger another write back to the keychain.
		Utils::ReadSecret(
			keychainService(), API_KEY_KEYCHAIN_KEY,
			[this](const QString& key) {
				if (mApiKey != key) {
					mApiKey = key;
					emit apiKeyChanged();
				}
			},
			{}, {}, this);
	}

	setupConnectionCheckTimer();
}

RedmineClient::~RedmineClient() { delete mNetworkManager; }

QString RedmineClient::baseUrl() const { return mBaseUrl; }

void RedmineClient::activatePeriodicConnectionCheck()
{
	if (mBaseUrl.isEmpty() || mApiKey.isEmpty()) {
		return;
	}

	mConnectionCheckTimer.start();
}

void RedmineClient::checkConnection()
{
	QUrlQuery query;
	query.addQueryItem("limit", 0);
	mNetworkManager->get(createRequest(ApiResources::ISSUES, query));
}

void RedmineClient::setBaseUrl(const QString& url)
{
	// createRequest() builds request URLs by concatenating mBaseUrl directly with endpoints such as
	// "issues.json", so mBaseUrl must always end with a slash or every request URL would be malformed.
	QString normalizedUrl = url;
	if (!normalizedUrl.isEmpty() && !normalizedUrl.endsWith('/')) {
		normalizedUrl += '/';
	}

	if (mBaseUrl != normalizedUrl) {
		mBaseUrl = normalizedUrl;
		Utils::SaveSetting("client/baseUrl", mBaseUrl);
		emit baseUrlChanged();
	}
}

QString RedmineClient::apiKey() const { return mApiKey; }

void RedmineClient::setApiKey(const QString& key)
{
	if (mApiKey != key) {
		mApiKey = key;
		Utils::WriteSecret(keychainService(), API_KEY_KEYCHAIN_KEY, mApiKey);
		emit apiKeyChanged();
	}
}

void RedmineClient::useSSL(bool useSSL)
{
	if (mUseSSL != useSSL) {
		mUseSSL = useSSL;
		mNetworkManager->setStrictTransportSecurityEnabled(useSSL);
		emit useSSLChanged();
	}
}

bool RedmineClient::loading() const { return mLoading; }

int RedmineClient::apiVersion() const { return mApiVersion; }

bool RedmineClient::hasApiVersion(int minVersion) const
{
	return mApiVersion != UNKNOWN_API_VERSION && mApiVersion >= minVersion;
}

void RedmineClient::detectApiVersion()
{
	setLoading(true);
	QUrlQuery query;
	query.addQueryItem("limit", "0");
	mNetworkManager->get(createRequest(ApiResources::ISSUES, query));
}

void RedmineClient::updateApiVersionFromReply(QNetworkReply* reply)
{
	if (!reply->hasRawHeader("X-Redmine-API-Version")) {
		return;
	}

	bool ok = false;
	int version = reply->rawHeader("X-Redmine-API-Version").toInt(&ok);
	if (ok && version != mApiVersion) {
		mApiVersion = version;
		emit apiVersionChanged();
	}
}

void RedmineClient::setLoading(bool loading)
{
	if (mLoading != loading) {
		mLoading = loading;
		emit loadingChanged();
	}
}

QNetworkRequest RedmineClient::createRequest(const QString& endpoint, const QUrlQuery& query)
{
	QUrl url(mBaseUrl + endpoint + "?" + query.toString());
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("X-Redmine-API-Key", mApiKey.toUtf8());
	return request;
}

QString RedmineClient::userErrorMessage(QNetworkReply* reply, const QJsonObject& response) const
{
	switch (reply->error()) {
		case QNetworkReply::HostNotFoundError:
			return tr("The Redmine server could not be found. Please check the server URL in the settings.");
		case QNetworkReply::ConnectionRefusedError:
			return tr("The Redmine server refused the connection.");
		case QNetworkReply::RemoteHostClosedError:
			return tr("The Redmine server closed the connection unexpectedly.");
		case QNetworkReply::TimeoutError:
		case QNetworkReply::OperationCanceledError:
			return tr("The connection to the Redmine server timed out.");
		case QNetworkReply::SslHandshakeFailedError:
			return tr("A secure connection to the Redmine server could not be established.");
		case QNetworkReply::TemporaryNetworkFailureError:
		case QNetworkReply::NetworkSessionFailedError:
			return tr("No network connection is available.");
		case QNetworkReply::AuthenticationRequiredError:
			return tr("Authentication failed. Please check your API key in the settings.");
		case QNetworkReply::ContentAccessDenied:
			return tr("Access denied. You are not allowed to perform this action.");
		default:
			break;
	}

	// Redmine answers validation failures (422 Unprocessable Entity) with {"errors": ["..."]}. Those
	// texts are already localized by the server according to the API user's language, so they are
	// passed through as-is.
	const QJsonArray errors = response.value(QStringLiteral("errors")).toArray();
	if (!errors.isEmpty()) {
		QStringList messages;
		for (const auto& error : errors) {
			messages.append(error.toString());
		}
		return tr("The Redmine server rejected the request: %1").arg(messages.join(QStringLiteral(", ")));
	}

	const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (httpStatus >= 500) {
		return tr("The Redmine server reported an internal error (HTTP %1).").arg(httpStatus);
	}

	// Fallback for everything not covered above; QNetworkReply::errorString() is translated through
	// Qt's own qtbase catalog.
	return tr("Network error: %1").arg(reply->errorString());
}

RedmineClient::ConnectionState RedmineClient::connectionState() const { return mConnectionState; }

void RedmineClient::setConnectionState(RedmineClient::ConnectionState newConnectionState)
{
	if (mConnectionState == newConnectionState) {
		return;
	}
	mConnectionState = newConnectionState;
	emit connectionStateChanged();
}

void RedmineClient::toggleConnectionState(ConnectionState newConnectionState)
{
	ConnectionState target = newConnectionState;
	switch (mConnectionState) {
		case OFFLINE:
			target = newConnectionState;
			break;
		case TEMPORARY_OFFLINE:
		case ONLINE:
			// We have been online before, so we only become temporary offline again
			target = OFFLINE == newConnectionState ? TEMPORARY_OFFLINE : newConnectionState;
			break;
	}
	setConnectionState(target);
}

void RedmineClient::createTimeEntry(int issueId, double hours, const QString& comments, const QString& spentOn,
	int activityId, const QVariantList& customFields, int requestTag)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["issue_id"] = issueId;
	timeEntry["spent_on"] = spentOn;
	timeEntry["hours"] = hours;
	timeEntry["comments"] = comments;
	if (activityId > 0) {
		timeEntry["activity_id"] = activityId;
	}
	if (!customFields.isEmpty()) {
		timeEntry["custom_fields"] = QJsonArray::fromVariantList(customFields);
	}

	// FIXME Currently not supported
	// TODO Add fields to UI
	//  timeEntry["user_id"] = 0;

	timeEntryData["time_entry"] = timeEntry;

	QJsonDocument doc(timeEntryData);
	QByteArray data = doc.toJson();

	QNetworkRequest request = createRequest(ApiResources::TIME_ENTRIES);
	request.setAttribute(REQUEST_TAG_ATTRIBUTE, requestTag);
	mNetworkManager->post(request, data);
}

void RedmineClient::updateTimeEntry(int timeEntryId, double hours, const QString& comments, const QString& spentOn,
	int activityId, const QVariantList& customFields, int requestTag)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["hours"] = hours;
	timeEntry["comments"] = comments;
	if (!spentOn.isEmpty()) {
		timeEntry["spent_on"] = spentOn;
	}
	if (activityId > 0) {
		timeEntry["activity_id"] = activityId;
	}
	if (!customFields.isEmpty()) {
		timeEntry["custom_fields"] = QJsonArray::fromVariantList(customFields);
	}
	timeEntryData["time_entry"] = timeEntry;

	QJsonDocument doc(timeEntryData);
	QByteArray data = doc.toJson();

	QNetworkRequest request = createRequest(ApiResources::TIME_ENTRY.arg(timeEntryId));
	request.setAttribute(REQUEST_TAG_ATTRIBUTE, requestTag);
	mNetworkManager->put(request, data);
}

void RedmineClient::getTimeEntries(int issueId)
{
	setLoading(true);
	QUrlQuery query;
	query.addQueryItem("issue_id", QString::number(issueId));
	mNetworkManager->get(createRequest(ApiResources::TIME_ENTRIES, query));
}

void RedmineClient::deleteTimeEntry(int timeEntryId)
{
	setLoading(true);
	mNetworkManager->deleteResource(createRequest(ApiResources::TIME_ENTRY.arg(timeEntryId)));
}

void RedmineClient::getIssue(int issueId, bool quiet)
{
	setLoading(true);
	QNetworkRequest request = createRequest(ApiResources::ISSUE.arg(QString::number(issueId)));
	request.setAttribute(QUIET_ATTRIBUTE, quiet);
	mNetworkManager->get(request);
}

void RedmineClient::getIssues(int projectId, int statusId, int limit, int offset)
{
	setLoading(true);

	QUrlQuery query;
	if (projectId > 0) {
		query.addQueryItem("project_id", QString::number(projectId));
	}
	if (statusId > 0) {
		query.addQueryItem("status_id", QString::number(statusId));
	}
	query.addQueryItem("limit", QString::number(limit));
	query.addQueryItem("offset", QString::number(offset));

	mNetworkManager->get(createRequest(ApiResources::ISSUES, query));
}

void RedmineClient::createIssue(
	int projectId, const QString& subject, const QString& description, int trackerId, int priorityId)
{
	setLoading(true);

	QJsonObject issue;
	issue["project_id"] = projectId;
	issue["subject"] = subject;
	if (!description.isEmpty()) {
		issue["description"] = description;
	}
	if (trackerId > 0) {
		issue["tracker_id"] = trackerId;
	}
	if (priorityId > 0) {
		issue["priority_id"] = priorityId;
	}

	QJsonObject issueData;
	issueData["issue"] = issue;

	QJsonDocument doc(issueData);
	mNetworkManager->post(createRequest(ApiResources::ISSUES), doc.toJson());
}

void RedmineClient::updateIssue(int issueId, const QString& subject, const QString& description, int statusId)
{
	setLoading(true);

	QJsonObject issue;
	if (!subject.isEmpty()) {
		issue["subject"] = subject;
	}
	if (!description.isEmpty()) {
		issue["description"] = description;
	}
	if (statusId > 0) {
		issue["status_id"] = statusId;
	}

	QJsonObject issueData;
	issueData["issue"] = issue;

	QJsonDocument doc(issueData);
	QString endpoint = ApiResources::ISSUE.arg(issueId);
	mNetworkManager->put(createRequest(endpoint), doc.toJson());
}

void RedmineClient::deleteIssue(int issueId)
{
	setLoading(true);
	mNetworkManager->deleteResource(createRequest(ApiResources::ISSUE.arg(issueId)));
}

void RedmineClient::getProject(int projectId)
{
	setLoading(true);
	mNetworkManager->get(createRequest(ApiResources::PROJECT.arg(projectId)));
}

void RedmineClient::getProjects(int limit, int offset)
{
	setLoading(true);

	QUrlQuery query;
	query.addQueryItem("limit", QString::number(limit));
	query.addQueryItem("offset", QString::number(offset));
	// The activities each project allows, which can differ from the shared ones, and its modules
	// (e.g. whether it tracks issues at all).
	query.addQueryItem("include", "time_entry_activities,enabled_modules");

	mNetworkManager->get(createRequest(ApiResources::PROJECTS, query));
}

void RedmineClient::getCurrentUser()
{
	setLoading(true);
	QUrlQuery query;
	query.addQueryItem("include", "memberships");
	QNetworkRequest request = createRequest(ApiResources::CURRENT_USER, query);
	request.setAttribute(QUIET_ATTRIBUTE, true);
	mNetworkManager->get(request);
}

void RedmineClient::getRole(int roleId)
{
	setLoading(true);
	QNetworkRequest request = createRequest(ApiResources::ROLE.arg(roleId));
	request.setAttribute(QUIET_ATTRIBUTE, true);
	mNetworkManager->get(request);
}

void RedmineClient::getOwnTimeEntries(const QString& from, const QString& to)
{
	++mOwnTimeEntriesGeneration;
	mOwnTimeEntriesFrom = from;
	mOwnTimeEntriesTo = to;
	mOwnTimeEntries = QJsonArray();
	requestOwnTimeEntriesPage(0);
}

void RedmineClient::requestOwnTimeEntriesPage(int offset)
{
	setLoading(true);

	QUrlQuery query;
	query.addQueryItem("user_id", "me");
	query.addQueryItem("from", mOwnTimeEntriesFrom);
	query.addQueryItem("to", mOwnTimeEntriesTo);
	query.addQueryItem("limit", QString::number(PAGE_SIZE));
	query.addQueryItem("offset", QString::number(offset));

	QNetworkRequest request = createRequest(ApiResources::TIME_ENTRIES, query);
	request.setAttribute(ROUTING_ATTRIBUTE, OwnTimeEntriesTag);
	request.setAttribute(OWN_TIME_ENTRIES_GENERATION_ATTRIBUTE, mOwnTimeEntriesGeneration);
	mNetworkManager->get(request);
}

void RedmineClient::handleOwnTimeEntriesReply(QNetworkReply* reply)
{
	if (reply->request().attribute(OWN_TIME_ENTRIES_GENERATION_ATTRIBUTE).toInt() != mOwnTimeEntriesGeneration) {
		return;
	}
	if (reply->error() != QNetworkReply::NoError) {
		qCWarning(lcRedmineClient) << "Could not fetch own time entries:" << reply->errorString();
		return;
	}

	const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
	const QJsonArray page = response["time_entries"].toArray();
	for (const QJsonValue& entry : page) {
		mOwnTimeEntries.append(entry);
	}

	const int nextOffset = response["offset"].toInt() + page.size();
	if (!page.isEmpty() && nextOffset < response["total_count"].toInt()) {
		requestOwnTimeEntriesPage(nextOffset);
		return;
	}
	emit ownTimeEntriesReceived(mOwnTimeEntries);
	mOwnTimeEntries = QJsonArray();
}

void RedmineClient::getAllProjects()
{
	++mProjectsGeneration;
	mCollectedProjects = QJsonArray();
	requestProjectsPage(0);
}

void RedmineClient::requestProjectsPage(int offset)
{
	setLoading(true);

	QUrlQuery query;
	query.addQueryItem("limit", QString::number(PAGE_SIZE));
	query.addQueryItem("offset", QString::number(offset));
	// As in getProjects(): the activities each project allows and its modules.
	query.addQueryItem("include", "time_entry_activities,enabled_modules");

	QNetworkRequest request = createRequest(ApiResources::PROJECTS, query);
	request.setAttribute(PROJECTS_GENERATION_ATTRIBUTE, mProjectsGeneration);
	mNetworkManager->get(request);
}

void RedmineClient::collectProjectsPage(const QJsonObject& response)
{
	const QJsonArray page = response["projects"].toArray();
	for (const QJsonValue& project : page) {
		mCollectedProjects.append(project);
	}

	const int nextOffset = response["offset"].toInt() + page.size();
	if (!page.isEmpty() && nextOffset < response["total_count"].toInt()) {
		requestProjectsPage(nextOffset);
		return;
	}
	emit projectsReceived(mCollectedProjects);
	mCollectedProjects = QJsonArray();
}

void RedmineClient::createProject(const QString& name, const QString& identifier, const QString& description)
{
	setLoading(true);

	QJsonObject project;
	project["name"] = name;
	project["identifier"] = identifier;
	if (!description.isEmpty()) {
		project["description"] = description;
	}

	QJsonObject projectData;
	projectData["project"] = project;

	QJsonDocument doc(projectData);
	mNetworkManager->post(createRequest(ApiResources::PROJECTS), doc.toJson());
}

void RedmineClient::updateProject(int projectId, const QString& name, const QString& description)
{
	setLoading(true);

	QJsonObject project;
	if (!name.isEmpty()) {
		project["name"] = name;
	}
	if (!description.isEmpty()) {
		project["description"] = description;
	}

	QJsonObject projectData;
	projectData["project"] = project;

	QJsonDocument doc(projectData);
	QString endpoint = ApiResources::PROJECT.arg(projectId);
	mNetworkManager->put(createRequest(endpoint), doc.toJson());
}

void RedmineClient::deleteProject(int projectId)
{
	setLoading(true);
	mNetworkManager->deleteResource(createRequest(ApiResources::PROJECT.arg(projectId)));
}

void RedmineClient::getEnumerations(EnumerationTypes enumerationType)
{
	setLoading(true);
	QString endpoint;
	switch (enumerationType) {
		case TIME_ENTRY_ACTIVITIES:
			endpoint = QString("enumerations/time_entry_activities.json");
			break;
		case ISSUE_PRIORITIES:
			endpoint = QString("enumerations/issue_priorities.json");
			break;
		case DOCUMENT_CATEGORIES:
			endpoint = QString("enumerations/document_categories.json");
			break;
		default:
			qWarning(lcRedmineClient())
				<< QStringLiteral("No valid Enumeration type is provided: <%0>").arg(enumerationType);
			return;
	}

	mNetworkManager->get(createRequest(endpoint));
}

void RedmineClient::getCustomFields()
{
	setLoading(true);
	mNetworkManager->get(createRequest(ApiResources::CUSTOM_FIELDS));
}

void RedmineClient::getTimeEntryCustomFields()
{
	QNetworkRequest request = createRequest(ApiResources::CUSTOM_FIELDS);
	request.setAttribute(ROUTING_ATTRIBUTE, TimeEntryCustomFieldsTag);
	request.setAttribute(CUSTOM_FIELDS_GENERATION_ATTRIBUTE, ++mCustomFieldsGeneration);
	mNetworkManager->get(request);
}

// Custom fields of time entries as seen on the entries themselves (id, name, value): used when
// /custom_fields.json is admin-only. The possible values are the ones in use, and the format is
// guessed from them.
static QJsonArray timeEntryCustomFieldsFromEntries(const QJsonArray& entries)
{
	QMap<int, QString> names;
	QMap<int, QStringList> values;
	for (const auto& entry : entries) {
		for (const auto& fieldValue : entry.toObject().value(QStringLiteral("custom_fields")).toArray()) {
			const QJsonObject field = fieldValue.toObject();
			const int id = field.value(QStringLiteral("id")).toInt();
			names.insert(id, field.value(QStringLiteral("name")).toString());
			QStringList& known = values[id];
			const QJsonValue value = field.value(QStringLiteral("value"));
			const QJsonArray used = value.isArray() ? value.toArray() : QJsonArray { value };
			for (const auto& item : used) {
				const QString text = item.toString();
				if (!text.isEmpty() && !known.contains(text)) {
					known.append(text);
				}
			}
		}
	}

	QJsonArray fields;
	for (auto it = names.cbegin(); it != names.cend(); ++it) {
		QStringList used = values.value(it.key());
		used.sort();
		const bool boolean = !used.isEmpty() && std::all_of(used.cbegin(), used.cend(), [](const QString& value) {
			return value == QStringLiteral("0") || value == QStringLiteral("1");
		});
		QJsonArray possibleValues;
		for (const auto& value : std::as_const(used)) {
			possibleValues.append(
				QJsonObject { { QStringLiteral("value"), value }, { QStringLiteral("label"), value } });
		}
		fields.append(QJsonObject { { QStringLiteral("id"), it.key() }, { QStringLiteral("name"), it.value() },
			{ QStringLiteral("customized_type"), QStringLiteral("time_entry") },
			{ QStringLiteral("field_format"), boolean ? QStringLiteral("bool") : QStringLiteral("list") },
			{ QStringLiteral("possible_values"), possibleValues } });
	}
	return fields;
}

void RedmineClient::handleTimeEntryCustomFieldsReply(QNetworkReply* reply, RequestTag tag)
{
	// Answers for an API key that has changed since (see getTimeEntryCustomFields()).
	const int generation = reply->request().attribute(CUSTOM_FIELDS_GENERATION_ATTRIBUTE).toInt();
	if (generation != mCustomFieldsGeneration) {
		return;
	}
	const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();

	if (reply->error() != QNetworkReply::NoError) {
		if (tag == TimeEntryCustomFieldsTag && reply->error() == QNetworkReply::ContentAccessDenied) {
			// Typically 403: only admins may list custom fields. Look at recent time entries instead.
			qCDebug(lcRedmineClient) << "Custom fields not listable (" << reply->errorString()
									 << "), deriving them from time entries";
			QUrlQuery query;
			query.addQueryItem("limit", "100");
			QNetworkRequest request = createRequest(ApiResources::TIME_ENTRIES, query);
			request.setAttribute(ROUTING_ATTRIBUTE, TimeEntryCustomFieldsFallbackTag);
			request.setAttribute(CUSTOM_FIELDS_GENERATION_ATTRIBUTE, generation);
			mNetworkManager->get(request);
		} else {
			qCWarning(lcRedmineClient) << "Could not determine time entry custom fields:" << reply->errorString();
		}
		return;
	}

	if (tag == TimeEntryCustomFieldsTag) {
		QJsonArray fields;
		for (const auto& field : response.value(QStringLiteral("custom_fields")).toArray()) {
			if (field.toObject().value(QStringLiteral("customized_type")).toString() == QStringLiteral("time_entry")) {
				fields.append(field);
			}
		}
		emit timeEntryCustomFieldsReceived(fields);
	} else {
		emit timeEntryCustomFieldsReceived(
			timeEntryCustomFieldsFromEntries(response.value(QStringLiteral("time_entries")).toArray()));
	}
}

void RedmineClient::updateConnectionStateFromReply(QNetworkReply* reply)
{
	if (reply->error() == QNetworkReply::NoError) {
		activatePeriodicConnectionCheck();
		setConnectionState(ConnectionState::ONLINE);
		return;
	}
	if (REASONS_FOR_CONNECTION_LOSS.contains(reply->error())) {
		toggleConnectionState(ConnectionState::OFFLINE);
		if (reply->error() == QNetworkReply::HostNotFoundError ||
			reply->error() == QNetworkReply::AuthenticationRequiredError) {
			mConnectionCheckTimer.stop();
		}
	}
}

void RedmineClient::handleNetworkReply(QNetworkReply* reply)
{
	reply->deleteLater();
	setLoading(false);
	updateApiVersionFromReply(reply);

	// Requests answered separately: kept out of the error reporting and the generic routing below
	// (the fallback's time entries must not show up as e.g. timeEntriesReceived()).
	const auto tag = static_cast<RequestTag>(reply->request().attribute(ROUTING_ATTRIBUTE).toInt());
	if (tag == TimeEntryCustomFieldsTag || tag == TimeEntryCustomFieldsFallbackTag) {
		updateConnectionStateFromReply(reply);
		handleTimeEntryCustomFieldsReply(reply, tag);
		return;
	}
	if (tag == OwnTimeEntriesTag) {
		updateConnectionStateFromReply(reply);
		handleOwnTimeEntriesReply(reply);
		return;
	}

	if (reply->error() != QNetworkReply::NoError) {
		QByteArray data = reply->readAll();
		QJsonDocument doc = QJsonDocument::fromJson(data);
		QJsonObject response = doc.object();

		if (reply->error() != QNetworkReply::ContentNotFoundError &&
			!reply->request().attribute(QUIET_ATTRIBUTE).toBool()) {
			emit errorOccurred(userErrorMessage(reply, response));
		}

		qWarning(lcRedmineClient) << QStringLiteral("[%0] %1: ").arg(reply->error()).arg(reply->errorString())
								  << QJsonDocument(response).toJson(QJsonDocument::Compact);

		if (reply->operation() == QNetworkAccessManager::PutOperation &&
			reply->url().path().contains("/time_entries/")) {
			emit timeEntryUpdateFailed(
				resourceIdFromPath(reply->url().path()), reply->request().attribute(REQUEST_TAG_ATTRIBUTE).toInt());
		}

		updateConnectionStateFromReply(reply);
		return;
	}

	updateConnectionStateFromReply(reply);

	QByteArray data = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject response = doc.object();

	QString path {};
	int id {};
	switch (reply->operation()) {
		case QNetworkAccessManager::HeadOperation:
			break;
		case QNetworkAccessManager::GetOperation:
			if (response.contains("time_entries")) {
				emit timeEntriesReceived(response["time_entries"].toArray());
			}
			if (response.contains("time_entry_activities")) {
				emit timeEntryActivitiesReceived(response["time_entry_activities"].toArray());
			}
			if (response.contains("issue_priorities")) {
				emit enumerationsReceived(response["issue_priorities"].toArray());
			}
			if (response.contains("document_categories")) {
				emit enumerationsReceived(response["document_categories"].toArray());
			}
			if (response.contains("custom_fields")) {
				emit customFieldsReceived(response["custom_fields"].toArray());
			}
			if (response.contains("issue")) {
				emit issueReceived(response["issue"].toObject());
			}
			if (response.contains("issues")) {
				emit issuesReceived(response["issues"].toArray());
			}
			if (response.contains("project")) {
				emit projectReceived(response["project"].toObject());
			}
			if (response.contains("user")) {
				emit currentUserReceived(response["user"].toObject());
			}
			if (response.contains("role")) {
				emit roleReceived(response["role"].toObject());
			}
			if (response.contains("projects")) {
				const QVariant generation = reply->request().attribute(PROJECTS_GENERATION_ATTRIBUTE);
				if (!generation.isValid()) {
					emit projectsReceived(response["projects"].toArray());
				} else if (generation.toInt() == mProjectsGeneration) {
					collectProjectsPage(response);
				}
			}
			break;
		case QNetworkAccessManager::PutOperation:
			path = reply->url().path();
			id = resourceIdFromPath(path);
			if (path.contains("/time_entries/")) {
				emit timeEntryUpdated(id, reply->request().attribute(REQUEST_TAG_ATTRIBUTE).toInt());
			} else if (path.contains("/issues/")) {
				emit issueUpdated(id);
			} else if (path.contains("/projects/")) {
				emit projectUpdated(id);
			}
			break;

		case QNetworkAccessManager::PostOperation:
			if (response.contains("time_entry")) {
				emit timeEntryCreated(
					response["time_entry"].toObject(), reply->request().attribute(REQUEST_TAG_ATTRIBUTE).toInt());
			}
			if (response.contains("issue")) {
				emit issueCreated(response["issue"].toObject());
			}
			if (response.contains("project")) {
				emit projectCreated(response["project"].toObject());
			}
			break;
		case QNetworkAccessManager::DeleteOperation:
			path = reply->url().path();
			id = resourceIdFromPath(path);
			if (path.contains("/time_entries/")) {
				emit timeEntryDeleted(id);
			} else if (path.contains("/issues/")) {
				emit issueDeleted(id);
			} else if (path.contains("/projects/")) {
				emit projectDeleted(id);
			}
			break;
		case QNetworkAccessManager::CustomOperation:
			break;
		case QNetworkAccessManager::UnknownOperation:
			break;
	}
}
}
