#include "redmineclient.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStringLiteral>
#include <QTranslator>
#include <QUrlQuery>
#include <QtLogging>
#include <algorithm>
#include <chrono>
#include <utility>

#include "logging.h"
#include "qsettingshelper.h"
#include "securestorage.h"

Q_LOGGING_CATEGORY(lcRedmineClient, "qredmine.client")
// Tracing of the single requests; at the "verbose" log level also what is sent and how a paged fetch goes on
Q_LOGGING_CATEGORY(lcRedmineNetwork, "qredmine.client.network")
// What the user entered or Redmine answered, only logged at the Debug level
Q_LOGGING_CATEGORY(lcRedmineContent, "qredmine.content")

namespace QRedmineClient {

// Key the API key is stored under within keychainService(), and the legacy QSettings key it used to
// be (insecurely) persisted under before it was migrated to the OS-native secure storage.
static const QString API_KEY_KEYCHAIN_KEY = QStringLiteral("apiKey");
static const QString LEGACY_API_KEY_SETTING = QStringLiteral("client/apiKey");
//! Request attribute carrying the requestTag of createTimeEntry()/updateTimeEntry() to the reply
static constexpr auto REQUEST_TAG_ATTRIBUTE = QNetworkRequest::User;
//! Request attribute carrying how a failure of the request is reported (a RedmineClient::Reporting)
static constexpr auto REPORTING_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);
//! Request attribute carrying the generation of the paged fetch (PagedFetch) or permission lookup
//! (mPermissionsGeneration) a reply belongs to, so replies of a superseded call are ignored
static constexpr auto GENERATION_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2);
//! Request attribute carrying a RedmineClient::RequestTag (routing of separately handled replies)
static constexpr auto ROUTING_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 3);
//! Request attribute carrying the getTimeEntryCustomFields() call (mCustomFieldsGeneration) a reply
//! belongs to
static constexpr auto CUSTOM_FIELDS_GENERATION_ATTRIBUTE =
	static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 4);
//! Request attribute carrying the RedmineClient::fetchKey() of a paged fetch's page
static constexpr auto FETCH_KEY_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 6);
//! Request attribute carrying the issue id of a getIssue() request
static constexpr auto ISSUE_ID_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 7);
//! Request attribute carrying the offset a page of a paged fetch was requested with
static constexpr auto PAGE_OFFSET_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 8);
//! Request attribute carrying the generation of the getIssuesById() batches (mIssueBatchGeneration) a
//! reply belongs to
static constexpr auto ISSUE_BATCH_GENERATION_ATTRIBUTE =
	static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 9);
//! Request attribute carrying the running number a request is traced with (see createRequest())
static constexpr auto REQUEST_ID_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 10);
//! Request attribute carrying the time a request was made, as milliseconds since the epoch
static constexpr auto REQUEST_START_ATTRIBUTE = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 11);
//! Dynamic property set on a read request's reply whose request was made with a base URL or API key that
//! has changed since; such a reply is dropped by handleNetworkReply() (see abortOutstandingRequests())
static constexpr auto SUPERSEDED_PROPERTY = "superseded";
//! A request is aborted (OperationCanceledError) when its transfer makes no progress for this long,
//! so a server that accepts connections but never answers can't block the client forever
static constexpr std::chrono::milliseconds REQUEST_TIMEOUT = std::chrono::seconds(30);
//! Interval between connection checks while the server answers
static constexpr int CONNECTION_CHECK_ONLINE_INTERVAL = 60000;
//! Interval before the first connection check, and after the first failed one; doubled with each
//! further failure up to CONNECTION_CHECK_MAX_INTERVAL
static constexpr int CONNECTION_CHECK_BASE_INTERVAL = 5000;
static constexpr int CONNECTION_CHECK_MAX_INTERVAL = 300000;
//! Page size used by getAllProjects() and getOwnTimeEntries(); 100 is the most Redmine returns per
//! request
static constexpr int PAGE_SIZE = 100;
//! A paged fetch ends after this many pages (20 000 items), whatever the server claims to have, so a
//! server or proxy that keeps returning the same page can't make the client fetch forever
static constexpr int MAX_PAGES = 200;
//! Most issues asked for by one getIssuesById() request, and the most such requests running at once
static constexpr int ISSUE_BATCH_SIZE = 100;
static constexpr int MAX_ISSUE_BATCHES_RUNNING = 2;
//! Longest wait a "Retry-After" header can ask for, in seconds
static constexpr int MAX_RETRY_AFTER = 86400;
//! A request taking longer than this (in milliseconds) is marked as slow in the log
static constexpr qint64 SLOW_REQUEST_MS = 5000;
//! Longest part of an error response's body written to the log (at the "debug" level only)
static constexpr int MAX_LOGGED_BODY_SIZE = 2000;

// Service identifier the API key is stored under in the OS-native secure storage (see securestorage.h).
// Derived from the application name rather than a fixed literal so that, e.g., the test binary
// (which sets its own QCoreApplication::applicationName(), see tests/main.cpp) never reads from or
// writes to the same keychain entry as the real application.
static QString keychainService()
{
	const QString appName = QCoreApplication::applicationName();
	// The fallback keeps its name from before the rename to Stride: keys stored by callers without an
	// application name are filed under it, and a new name would make them unreadable.
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

bool RedmineClient::serverNotReached(QNetworkReply::NetworkError error)
{
	switch (error) {
		case QNetworkReply::ConnectionRefusedError:
		case QNetworkReply::RemoteHostClosedError:
		case QNetworkReply::HostNotFoundError:
		case QNetworkReply::TimeoutError:
		case QNetworkReply::OperationCanceledError:
		case QNetworkReply::TemporaryNetworkFailureError:
		case QNetworkReply::NetworkSessionFailedError:
		case QNetworkReply::UnknownNetworkError:
		case QNetworkReply::ProxyConnectionClosedError:
		case QNetworkReply::ProxyTimeoutError:
		// Gateway errors (502, 504) and 503 while Redmine restarts or is overloaded
		case QNetworkReply::UnknownServerError:
		case QNetworkReply::ServiceUnavailableError:
			return true;
		default:
			return false;
	}
}

bool RedmineClient::isConnectionLoss(QNetworkReply::NetworkError error)
{
	return serverNotReached(error) || error == QNetworkReply::AuthenticationRequiredError;
}

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
	// Single-shot: re-armed once a probe has finished, so probes never pile up behind a slow server
	mConnectionCheckTimer.setSingleShot(true);
	connect(&mConnectionCheckTimer, &QTimer::timeout, this, &RedmineClient::checkConnection);

	// First, so that what was sent with the old settings is gone before anything is sent with the new ones
	connect(this, &RedmineClient::baseUrlChanged, this, &RedmineClient::abortOutstandingRequests);
	connect(this, &RedmineClient::apiKeyChanged, this, &RedmineClient::abortOutstandingRequests);
	connect(this, &RedmineClient::baseUrlChanged, this, &RedmineClient::activatePeriodicConnectionCheck);
	connect(this, &RedmineClient::apiKeyChanged, this, &RedmineClient::activatePeriodicConnectionCheck);
	activatePeriodicConnectionCheck();
}

RedmineClient::RedmineClient(QObject* parent)
	: QObject(parent)
	, mLoading(false)
	, mNetworkManager(new QNetworkAccessManager(this))
{
	mNetworkManager->setTransferTimeout(REQUEST_TIMEOUT);
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
				// Unless a key was set in the meantime, that one is newer
				if (!mApiKeyLoaded && mApiKey != key) {
					mApiKey = key;
					emit apiKeyChanged();
				}
				setApiKeyLoaded();
			},
			[this]() { setApiKeyLoaded(); }, [this](const QString&) { setApiKeyLoaded(); }, this);
	}

	setupConnectionCheckTimer();
}

RedmineClient::~RedmineClient() { delete mNetworkManager; }

QString RedmineClient::baseUrl() const { return mBaseUrl; }

void RedmineClient::activatePeriodicConnectionCheck()
{
	// A probe still running was made with the previous URL / key; its result is of no interest anymore
	if (mConnectionCheckReply) {
		QPointer<QNetworkReply> staleReply = mConnectionCheckReply;
		mConnectionCheckReply = nullptr;
		// Before abort(): it emits finished() at once
		staleReply->setProperty(SUPERSEDED_PROPERTY, true);
		staleReply->abort();
	}
	mConnectionCheckFailures = 0;
	if (mBaseUrl.isEmpty() || mApiKey.isEmpty()) {
		mConnectionCheckTimer.stop();
		QR_DEBUG(lcRedmineClient, QStringLiteral("Connection checks stopped: no server URL or API key"),
			{ { "hasUrl", !mBaseUrl.isEmpty() }, { "hasApiKey", !mApiKey.isEmpty() } });
		return;
	}
	scheduleConnectionCheck(CONNECTION_CHECK_BASE_INTERVAL);
}

void RedmineClient::scheduleConnectionCheck(int interval)
{
	QR_DEBUG(lcRedmineClient, QStringLiteral("Next connection check in %1 ms").arg(interval),
		{ { "intervalMs", interval }, { "failures", mConnectionCheckFailures } });
	mConnectionCheckTimer.start(interval);
}

int RedmineClient::connectionCheckInterval() const { return mConnectionCheckTimer.interval(); }

bool RedmineClient::connectionCheckRunning() const { return !mConnectionCheckReply.isNull(); }

void RedmineClient::checkConnection()
{
	if (mConnectionCheckReply || mBaseUrl.isEmpty() || mApiKey.isEmpty()) {
		return;
	}

	// Only the API version header and the status matter, so ask for as little data as possible (Redmine
	// ignores limit=0 and returns the default page size then)
	QUrlQuery query;
	query.addQueryItem("limit", "1");
	auto request = createRequest(ApiResources::ISSUES, query);
	request.setAttribute(ROUTING_ATTRIBUTE, ConnectionCheckTag);
	mConnectionCheckReply = mNetworkManager->get(request);
}

void RedmineClient::handleConnectionCheckReply(QNetworkReply* reply)
{
	updateApiVersionFromReply(reply);
	// Still the pending probe here, so updateConnectionStateFromReply() leaves the scheduling to us
	updateConnectionStateFromReply(reply);
	mConnectionCheckReply = nullptr;
	// Any failed probe means the server can't be used right now, also for errors that don't count
	// as a lost connection for other requests (e.g. 500)
	if (reply->error() != QNetworkReply::NoError && !isConnectionLoss(reply->error())) {
		toggleConnectionState(ConnectionState::OFFLINE);
	}

	if (reply->error() == QNetworkReply::NoError) {
		mConnectionCheckFailures = 0;
		scheduleConnectionCheck(CONNECTION_CHECK_ONLINE_INTERVAL);
	} else if (reply->error() != QNetworkReply::AuthenticationRequiredError) {
		// Back off 5 s, 10 s, 20 s, ... so an unreachable server isn't hammered, but is found again soon
		// after it recovers (e.g. when the network is back after waking from sleep)
		const int interval =
			std::min(CONNECTION_CHECK_BASE_INTERVAL << mConnectionCheckFailures, CONNECTION_CHECK_MAX_INTERVAL);
		if (interval < CONNECTION_CHECK_MAX_INTERVAL) {
			++mConnectionCheckFailures;
		}
		scheduleConnectionCheck(interval);
	}
	// Else the API key is wrong: probing again won't change that, only a new key does
	// (activatePeriodicConnectionCheck() via apiKeyChanged)
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

bool RedmineClient::apiKeyLoaded() const { return mApiKeyLoaded; }

void RedmineClient::setApiKeyLoaded()
{
	if (!mApiKeyLoaded) {
		mApiKeyLoaded = true;
		emit apiKeyLoadedChanged();
	}
}

void RedmineClient::setApiKey(const QString& key)
{
	if (mApiKey != key) {
		mApiKey = key;
		// Clearing the key deletes it from the secure storage rather than storing an empty one
		if (mApiKey.isEmpty()) {
			Utils::DeleteSecret(keychainService(), API_KEY_KEYCHAIN_KEY);
		} else {
			Utils::WriteSecret(keychainService(), API_KEY_KEYCHAIN_KEY, mApiKey);
		}
		emit apiKeyChanged();
	}
	// A key set explicitly wins over whatever the keychain read may still deliver
	setApiKeyLoaded();
}

void RedmineClient::abortOutstandingRequests()
{
	// Nothing of the queued batches was sent yet, or was sent with the old settings
	const qsizetype droppedBatches = mIssueBatchQueue.size();
	mIssueBatchQueue.clear();
	mIssueBatchesRunning = 0;
	// Replies of running batches that are finished already, but not delivered yet, mustn't count anymore
	++mIssueBatchGeneration;
	for (PagedFetch& fetch : mPagedFetches) {
		fetch.collected = QJsonArray();
	}
	int aborted = 0;
	const auto replies = mNetworkManager->findChildren<QNetworkReply*>(Qt::FindDirectChildrenOnly);
	for (QNetworkReply* reply : replies) {
		// Writes are left alone: the server may have received them already, and reporting them as failed
		// would have them sent again (duplicate time entries). They end by themselves (transfer timeout).
		const auto operation = reply->operation();
		const bool read =
			operation == QNetworkAccessManager::GetOperation || operation == QNetworkAccessManager::HeadOperation;
		if (read && !reply->isFinished()) {
			// Before abort(): it emits finished() at once
			reply->setProperty(SUPERSEDED_PROPERTY, true);
			++aborted;
			reply->abort();
		}
	}
	if (aborted > 0 || droppedBatches > 0) {
		QR_INFO(lcRedmineClient,
			QStringLiteral("Server URL or API key changed: aborted %1 requests, dropped %2 queued issue requests")
				.arg(aborted)
				.arg(droppedBatches),
			{ { "aborted", aborted }, { "droppedBatches", static_cast<int>(droppedBatches) } });
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
	query.addQueryItem("limit", "1");
	QNetworkRequest request = createRequest(ApiResources::ISSUES, query);
	setReporting(request, Reporting::UnlessNotReached);
	mNetworkManager->get(request);
}

void RedmineClient::updateApiVersionFromReply(QNetworkReply* reply)
{
	if (!reply->hasRawHeader("X-Redmine-API-Version")) {
		return;
	}

	bool ok = false;
	int version = reply->rawHeader("X-Redmine-API-Version").toInt(&ok);
	if (ok && version != mApiVersion) {
		QR_INFO(lcRedmineClient, QStringLiteral("Redmine API version is %1").arg(version),
			{ { "apiVersion", version }, { "previous", mApiVersion } });
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

	// For the trace in the log, see logFinishedReply(). The API key is a header and never logged.
	const int requestId = ++mLastRequestId;
	request.setAttribute(REQUEST_ID_ATTRIBUTE, requestId);
	request.setAttribute(REQUEST_START_ATTRIBUTE, QDateTime::currentMSecsSinceEpoch());
	QR_DEBUG(lcRedmineNetwork,
		QStringLiteral("Request #%1 sent: %2").arg(requestId).arg(endpoint + '?' + query.toString()),
		{ { "id", requestId }, { "path", endpoint }, { "query", query.toString() } });
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
		// A request aborted by the transfer timeout reports OperationCanceledError; the only other
		// abort() is of superseded connection checks, which are never reported
		case QNetworkReply::TimeoutError:
		case QNetworkReply::OperationCanceledError:
			return tr("The Redmine server didn't answer in time.");
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
	const auto states = QMetaEnum::fromType<RedmineClient::ConnectionState>();
	const QString from = QString::fromLatin1(states.valueToKey(mConnectionState));
	const QString to = QString::fromLatin1(states.valueToKey(newConnectionState));
	QR_INFO(lcRedmineClient, QStringLiteral("Connection state changed from %1 to %2").arg(from, to),
		{ { "from", from }, { "to", to } });
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
	setReporting(request, Reporting::UnlessUnanswered);
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

void RedmineClient::getIssue(int issueId, bool quiet, bool withAllowedStatuses)
{
	setLoading(true);
	QUrlQuery query;
	if (withAllowedStatuses) {
		query.addQueryItem("include", "allowed_statuses");
	}
	QNetworkRequest request = createRequest(ApiResources::ISSUE.arg(QString::number(issueId)), query);
	setReporting(request, reportingForQuiet(quiet));
	request.setAttribute(ISSUE_ID_ATTRIBUTE, issueId);
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

void RedmineClient::getIssuesById(const QList<int>& issueIds, bool quiet)
{
	QList<int> ids;
	for (const int id : issueIds) {
		if (id > 0 && !ids.contains(id)) {
			ids.append(id);
		}
	}
	for (qsizetype i = 0; i < ids.size(); i += ISSUE_BATCH_SIZE) {
		mIssueBatchQueue.append({ ids.mid(i, ISSUE_BATCH_SIZE), reportingForQuiet(quiet) });
	}
	sendIssueBatches();
}

void RedmineClient::sendIssueBatches()
{
	while (mIssueBatchesRunning < MAX_ISSUE_BATCHES_RUNNING && !mIssueBatchQueue.isEmpty()) {
		const IssueBatch batch = mIssueBatchQueue.takeFirst();
		QStringList ids;
		for (const int id : batch.ids) {
			ids.append(QString::number(id));
		}
		QUrlQuery query;
		query.addQueryItem("issue_id", ids.join(','));
		query.addQueryItem("status_id", "*");
		query.addQueryItem("limit", QString::number(ISSUE_BATCH_SIZE));

		setLoading(true);
		QNetworkRequest request = createRequest(ApiResources::ISSUES, query);
		request.setAttribute(ROUTING_ATTRIBUTE, IssuesBatchTag);
		setReporting(request, batch.reporting);
		request.setAttribute(ISSUE_BATCH_GENERATION_ATTRIBUTE, mIssueBatchGeneration);
		++mIssueBatchesRunning;
		QR_DEBUG(lcRedmineNetwork, QStringLiteral("Fetching %1 issues in one request").arg(batch.ids.size()),
			{ { "ids", batch.ids.size() }, { "running", mIssueBatchesRunning },
				{ "queued", static_cast<int>(mIssueBatchQueue.size()) } });
		mNetworkManager->get(request);
	}
}

void RedmineClient::handleIssuesBatchReply(QNetworkReply* reply)
{
	// Of a batch sent before the settings changed: its slot was given up by abortOutstandingRequests()
	if (reply->request().attribute(ISSUE_BATCH_GENERATION_ATTRIBUTE).toInt() != mIssueBatchGeneration) {
		return;
	}
	--mIssueBatchesRunning;
	updateConnectionStateFromReply(reply);
	const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
	if (reply->error() != QNetworkReply::NoError) {
		qCWarning(lcRedmineClient) << "Could not fetch issues:" << reply->errorString();
		if (shouldReport(reply, reportingOf(reply))) {
			reportError(reply, response);
		}
	} else {
		for (const QJsonValue& issue : response["issues"].toArray()) {
			emit issueReceived(issue.toObject());
		}
	}
	sendIssueBatches();
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
	// Starts a new permission lookup: replies to the previous one (e.g. for another API key) are
	// ignored from now on.
	++mPermissionsGeneration;
	QUrlQuery query;
	query.addQueryItem("include", "memberships");
	requestPermissionData(createRequest(ApiResources::CURRENT_USER, query));
}

void RedmineClient::getRoles() { requestPermissionData(createRequest(ApiResources::ROLES)); }

void RedmineClient::getRole(int roleId) { requestPermissionData(createRequest(ApiResources::ROLE.arg(roleId))); }

void RedmineClient::requestPermissionData(QNetworkRequest request)
{
	setLoading(true);
	request.setAttribute(ROUTING_ATTRIBUTE, PermissionsTag);
	request.setAttribute(GENERATION_ATTRIBUTE, mPermissionsGeneration);
	mNetworkManager->get(request);
}

void RedmineClient::handlePermissionsReply(QNetworkReply* reply)
{
	if (reply->request().attribute(GENERATION_ATTRIBUTE).toInt() != mPermissionsGeneration) {
		return;
	}
	if (reply->error() != QNetworkReply::NoError) {
		qCWarning(lcRedmineClient) << "Could not fetch" << reply->url().path() << ":" << reply->errorString();
		emit permissionLookupFailed();
		return;
	}
	const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
	if (response.contains("user")) {
		emit currentUserReceived(response["user"].toObject());
	} else if (response.contains("roles")) {
		emit rolesReceived(response["roles"].toArray());
	} else if (response.contains("role")) {
		emit roleReceived(response["role"].toObject());
	}
}

void RedmineClient::getOwnTimeEntries(const QString& from, const QString& to)
{
	QUrlQuery query;
	query.addQueryItem("user_id", "me");
	query.addQueryItem("from", from);
	query.addQueryItem("to", to);
	startPagedFetch(
		OwnTimeEntriesTag, ApiResources::TIME_ENTRIES, query, QStringLiteral("time_entries"), Reporting::Never);
}

void RedmineClient::getTimeEntryReport(const QString& from, const QString& to, int purpose)
{
	QUrlQuery query;
	query.addQueryItem("user_id", "me");
	query.addQueryItem("from", from);
	query.addQueryItem("to", to);
	startPagedFetch(TimeEntryReportTag, ApiResources::TIME_ENTRIES, query, QStringLiteral("time_entries"),
		Reporting::Never, purpose);
}

void RedmineClient::getAllProjects(bool userRequested)
{
	QUrlQuery query;
	// As in getProjects(): the activities each project allows and its modules.
	query.addQueryItem("include", "time_entry_activities,enabled_modules");
	startPagedFetch(AllProjectsTag, ApiResources::PROJECTS, query, QStringLiteral("projects"),
		userRequested ? Reporting::Always : Reporting::UnlessNotReached);
}

void RedmineClient::startPagedFetch(RequestTag tag, const QString& resource, const QUrlQuery& query, const QString& key,
	Reporting reporting, int purpose)
{
	const int fetchKey = RedmineClient::fetchKey(tag, purpose);
	PagedFetch& fetch = mPagedFetches[fetchKey];
	if (fetch.running) {
		QR_DEBUG(lcRedmineNetwork, QStringLiteral("Fetch of %1 superseded by a new one").arg(key),
			{ { "key", key }, { "pages", fetch.pages } });
	}
	fetch = PagedFetch { tag, purpose, resource, query, key, reporting, fetch.generation + 1, 0, {} };
	QR_DEBUG(lcRedmineNetwork, QStringLiteral("Fetching all %1 page by page").arg(key),
		{ { "key", key }, { "purpose", purpose }, { "generation", fetch.generation } });
	requestPage(fetchKey, 0);
}

void RedmineClient::requestPage(int key, int offset)
{
	setLoading(true);
	PagedFetch& fetch = mPagedFetches[key];
	++fetch.pages;
	fetch.running = true;

	QR_DEBUG(lcRedmineNetwork, QStringLiteral("Requesting page %1 of %2").arg(fetch.pages).arg(fetch.key),
		{ { "key", fetch.key }, { "page", fetch.pages }, { "offset", offset } });

	QUrlQuery query = fetch.query;
	query.addQueryItem("limit", QString::number(PAGE_SIZE));
	query.addQueryItem("offset", QString::number(offset));

	QNetworkRequest request = createRequest(fetch.resource, query);
	request.setAttribute(ROUTING_ATTRIBUTE, fetch.tag);
	request.setAttribute(FETCH_KEY_ATTRIBUTE, key);
	request.setAttribute(GENERATION_ATTRIBUTE, fetch.generation);
	request.setAttribute(PAGE_OFFSET_ATTRIBUTE, offset);
	mNetworkManager->get(request);
}

void RedmineClient::handlePagedReply(QNetworkReply* reply)
{
	const int key = reply->request().attribute(FETCH_KEY_ATTRIBUTE).toInt();
	PagedFetch& fetch = mPagedFetches[key];
	if (reply->request().attribute(GENERATION_ATTRIBUTE).toInt() != fetch.generation) {
		return;
	}
	// Running again if the next page is requested
	fetch.running = false;
	const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
	if (reply->error() != QNetworkReply::NoError) {
		qCWarning(lcRedmineClient) << "Could not fetch" << fetch.key << ":" << reply->errorString();
		if (shouldReport(reply, fetch.reporting)) {
			reportError(reply, response);
		}
		if (fetch.tag == TimeEntryReportTag) {
			emit timeEntryReportFailed(
				fetch.query.queryItemValue("from"), fetch.query.queryItemValue("to"), fetch.purpose);
		}
		return;
	}

	const QJsonArray page = response[fetch.key].toArray();
	for (const QJsonValue& item : page) {
		fetch.collected.append(item);
	}

	// From the offset asked for, not the one the server reports: a server or proxy that leaves it out
	// would have the same page fetched again and again
	const int nextOffset = reply->request().attribute(PAGE_OFFSET_ATTRIBUTE).toInt() + page.size();
	if (!page.isEmpty() && nextOffset < response["total_count"].toInt()) {
		if (fetch.pages < MAX_PAGES) {
			requestPage(key, nextOffset);
			return;
		}
		QR_WARNING(lcRedmineClient,
			QStringLiteral("Stopped fetching %1 after %2 pages").arg(fetch.key).arg(fetch.pages),
			{ { "key", fetch.key }, { "pages", fetch.pages }, { "items", fetch.collected.size() },
				{ "total", response["total_count"].toInt() } });
		if (fetch.tag == OwnTimeEntriesTag) {
			// Linked against a truncated list, pending entries would be submitted again
			fetch.collected = QJsonArray();
			return;
		}
		if (fetch.tag == TimeEntryReportTag) {
			// An incomplete report would show wrong totals
			fetch.collected = QJsonArray();
			emit timeEntryReportFailed(
				fetch.query.queryItemValue("from"), fetch.query.queryItemValue("to"), fetch.purpose);
			return;
		}
	}
	const QJsonArray items = std::exchange(fetch.collected, QJsonArray());
	QR_DEBUG(lcRedmineNetwork,
		QStringLiteral("Fetched %1 %2 in %3 pages").arg(items.size()).arg(fetch.key).arg(fetch.pages),
		{ { "key", fetch.key }, { "items", items.size() }, { "pages", fetch.pages } });
	if (fetch.tag == AllProjectsTag) {
		emit projectsReceived(items);
	} else if (fetch.tag == OwnTimeEntriesTag) {
		emit ownTimeEntriesReceived(items);
	} else if (fetch.tag == TimeEntryReportTag) {
		emit timeEntryReportReceived(
			items, fetch.query.queryItemValue("from"), fetch.query.queryItemValue("to"), fetch.purpose);
	}
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
	QString endpoint;
	switch (enumerationType) {
		case TIME_ENTRY_ACTIVITIES:
			endpoint = QStringLiteral("enumerations/time_entry_activities.json");
			break;
		case ISSUE_PRIORITIES:
			endpoint = QStringLiteral("enumerations/issue_priorities.json");
			break;
		case DOCUMENT_CATEGORIES:
			endpoint = QStringLiteral("enumerations/document_categories.json");
			break;
		default:
			qWarning(lcRedmineClient())
				<< QStringLiteral("No valid Enumeration type is provided: <%0>").arg(enumerationType);
			return;
	}

	setLoading(true);
	QNetworkRequest request = createRequest(endpoint);
	setReporting(request, Reporting::UnlessNotReached);
	mNetworkManager->get(request);
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

// Seconds from the "Retry-After" header of a reply, which holds either a number of seconds or an HTTP
// date; 0 if there is none, it's in the past or not understood
static int retryAfterFromReply(QNetworkReply* reply)
{
	const QByteArray value = reply->rawHeader("Retry-After").trimmed();
	if (value.isEmpty()) {
		return 0;
	}
	const auto logResult = [&value](int seconds) {
		QR_INFO(lcRedmineClient, QStringLiteral("Server asks to wait %1 s before trying again").arg(seconds),
			{ { "retryAfterHeader", QString::fromLatin1(value) }, { "seconds", seconds } });
		return seconds;
	};
	bool isNumber = false;
	qint64 seconds = value.toLongLong(&isNumber);
	if (!isNumber) {
		// An HTTP date always ends in "GMT", which Qt's RFC 2822 parsing only accepts as "+0000"
		QString text = QString::fromLatin1(value);
		if (text.endsWith(QLatin1String(" GMT"))) {
			text = text.chopped(4) + QLatin1String(" +0000");
		}
		const QDateTime date = QDateTime::fromString(text, Qt::RFC2822Date);
		if (!date.isValid()) {
			return logResult(0);
		}
		seconds = QDateTime::currentDateTimeUtc().secsTo(date);
	}
	return logResult(static_cast<int>(std::clamp<qint64>(seconds, 0, MAX_RETRY_AFTER)));
}

void RedmineClient::updateConnectionStateFromReply(QNetworkReply* reply)
{
	if (reply->error() == QNetworkReply::NoError) {
		setConnectionState(ConnectionState::ONLINE);
		// The server answers again: leave the backoff, and resume checking if it was stopped. A probe's
		// own reply schedules the next one itself, and so does one still running.
		const bool isProbe = reply == mConnectionCheckReply;
		const bool canCheck = !mBaseUrl.isEmpty() && !mApiKey.isEmpty();
		if (!isProbe && canCheck) {
			const bool backingOff = mConnectionCheckFailures > 0;
			mConnectionCheckFailures = 0;
			if (!mConnectionCheckReply && (!mConnectionCheckTimer.isActive() || backingOff)) {
				scheduleConnectionCheck(CONNECTION_CHECK_ONLINE_INTERVAL);
			}
		}
		return;
	}
	if (isConnectionLoss(reply->error())) {
		toggleConnectionState(ConnectionState::OFFLINE);
		// Other losses (no host, no network, timeout) pass by themselves, so the check keeps going with
		// backoff; a rejected API key doesn't, it needs the user to enter a new one.
		if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
			mConnectionCheckTimer.stop();
		}
	}
}

void RedmineClient::updateLoading()
{
	const auto replies = mNetworkManager->findChildren<QNetworkReply*>(Qt::FindDirectChildrenOnly);
	setLoading(std::any_of(replies.cbegin(), replies.cend(), [](QNetworkReply* reply) {
		// Connection checks run in the background and don't count as loading
		return !reply->isFinished() && reply->request().attribute(ROUTING_ATTRIBUTE).toInt() != ConnectionCheckTag;
	}));
}

int RedmineClient::requestId(QNetworkReply* reply) { return reply->request().attribute(REQUEST_ID_ATTRIBUTE).toInt(); }

void RedmineClient::logFinishedReply(QNetworkReply* reply)
{
	const QNetworkRequest request = reply->request();
	const auto tag = static_cast<RequestTag>(request.attribute(ROUTING_ATTRIBUTE).toInt());
	const bool superseded = reply->property(SUPERSEDED_PROPERTY).toBool();
	const QNetworkReply::NetworkError error = reply->error();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const qint64 started = request.attribute(REQUEST_START_ATTRIBUTE).toLongLong();
	const qint64 durationMs = started > 0 ? QDateTime::currentMSecsSinceEpoch() - started : -1;
	const bool slow = durationMs > SLOW_REQUEST_MS;

	// A failed connection check is what happens while the server is unreachable, and a missing resource
	// (404) is often an answer in its own right; neither is worth a warning
	const bool isProbe = tag == ConnectionCheckTag;
	const bool failed = error != QNetworkReply::NoError && !superseded;
	QtMsgType type = QtInfoMsg;
	if (failed && !isProbe && error != QNetworkReply::ContentNotFoundError) {
		type = QtWarningMsg;
	} else if (isProbe && !failed && !slow) {
		type = QtDebugMsg;
	}
	// Most replies are filtered out, so nothing is built for them
	if (!lcRedmineNetwork().isEnabled(type)) {
		return;
	}

	QString method;
	switch (reply->operation()) {
		case QNetworkAccessManager::GetOperation:
			method = QStringLiteral("GET");
			break;
		case QNetworkAccessManager::PutOperation:
			method = QStringLiteral("PUT");
			break;
		case QNetworkAccessManager::PostOperation:
			method = QStringLiteral("POST");
			break;
		case QNetworkAccessManager::DeleteOperation:
			method = QStringLiteral("DELETE");
			break;
		case QNetworkAccessManager::HeadOperation:
			method = QStringLiteral("HEAD");
			break;
		default:
			method = QString::fromLatin1(request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray());
			break;
	}
	static const QStringList routingNames { QStringLiteral("none"), QStringLiteral("customFields"),
		QStringLiteral("customFieldsFallback"), QStringLiteral("allProjects"), QStringLiteral("ownTimeEntries"),
		QStringLiteral("timeEntryReport"), QStringLiteral("permissions"), QStringLiteral("connectionCheck"),
		QStringLiteral("issuesBatch") };
	const QString routing = routingNames.value(tag);
	const QString path = reply->url().path();

	QJsonObject fields { { "id", requestId(reply) }, { "method", method }, { "path", path },
		{ "query", reply->url().query() }, { "status", status }, { "durationMs", durationMs },
		{ "bytes", reply->bytesAvailable() } };
	if (tag != NoTag) {
		fields["tag"] = routing;
	}
	if (error != QNetworkReply::NoError) {
		fields["error"] = QString::fromLatin1(QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
		fields["errorString"] = reply->errorString();
	}
	if (superseded) {
		fields["superseded"] = true;
	} else if (error == QNetworkReply::TimeoutError || error == QNetworkReply::OperationCanceledError) {
		fields["timeout"] = true;
	}
	if (slow) {
		fields["slow"] = true;
	}

	QString message = QStringLiteral("%1 %2 -> ").arg(method, path);
	message += status > 0 ? QString::number(status)
						  : QString::fromLatin1(QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
	message += QStringLiteral(" (%1 ms)").arg(durationMs);

	QR_LOG(lcRedmineNetwork, type, message, fields);
}

RedmineClient::Reporting RedmineClient::reportingOf(QNetworkReply* reply)
{
	return static_cast<Reporting>(
		reply->request().attribute(REPORTING_ATTRIBUTE, static_cast<int>(Reporting::Always)).toInt());
}

void RedmineClient::setReporting(QNetworkRequest& request, Reporting reporting)
{
	request.setAttribute(REPORTING_ATTRIBUTE, static_cast<int>(reporting));
}

bool RedmineClient::shouldReport(QNetworkReply* reply, Reporting reporting) const
{
	// Without a server set up (e.g. on first start) every request fails; that's no news.
	if (reporting == Reporting::Never || mBaseUrl.isEmpty()) {
		return false;
	}
	// Not finding something is up to the caller to tell (e.g. issueNotFound())
	if (reply->error() == QNetworkReply::ContentNotFoundError) {
		return false;
	}
	switch (reporting) {
		case Reporting::UnlessUnanswered:
			// E.g. a time entry that got no answer (or only a gateway's 502/503/504) stays queued in the app
			// and is sent again later; one Redmine refused is reported.
			return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 0 &&
				!serverNotReached(reply->error());
		case Reporting::UnlessNotReached:
			// The connection state already shows that the server isn't usable
			return !serverNotReached(reply->error());
		default:
			return true;
	}
}

RedmineClient::Reporting RedmineClient::reportingForQuiet(bool quiet)
{
	return quiet ? Reporting::Never : Reporting::Always;
}

void RedmineClient::reportError(QNetworkReply* reply, const QJsonObject& response)
{
	const QString message = userErrorMessage(reply, response);
	// The message may hold what Redmine echoed of the user's input, so it's only logged in the content category
	QR_DEBUG(
		lcRedmineContent, QStringLiteral("Showing error to the user: %1").arg(message), { { "id", requestId(reply) } });
	QR_WARNING(lcRedmineClient, QStringLiteral("Showing an error to the user"),
		{ { "id", requestId(reply) }, { "path", reply->url().path() },
			{ "status", reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() },
			{ "error",
				QString::fromLatin1(QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error())) } });
	emit errorOccurred(message);
}

void RedmineClient::handleNetworkReply(QNetworkReply* reply)
{
	reply->deleteLater();
	logFinishedReply(reply);
	// Loading ends once no request is left, checked after this reply was handled: handling it may
	// start follow-up requests (e.g. the next page), so loading doesn't flicker in between.
	const auto updateLoadingWhenDone = qScopeGuard([this]() { updateLoading(); });

	const auto tag = static_cast<RequestTag>(reply->request().attribute(ROUTING_ATTRIBUTE).toInt());
	// Aborted because the base URL or API key changed: nothing of it is of interest anymore, no error,
	// connection state or data. Only what would otherwise be waited for forever is told.
	if (reply->property(SUPERSEDED_PROPERTY).toBool()) {
		if (tag == TimeEntryReportTag) {
			const PagedFetch fetch = mPagedFetches.value(reply->request().attribute(FETCH_KEY_ATTRIBUTE).toInt());
			if (reply->request().attribute(GENERATION_ATTRIBUTE).toInt() == fetch.generation) {
				emit timeEntryReportFailed(
					fetch.query.queryItemValue("from"), fetch.query.queryItemValue("to"), fetch.purpose);
			}
		}
		return;
	}
	// Connection checks only update the connection state and API version, they never show up as data
	// or errors
	if (tag == ConnectionCheckTag) {
		handleConnectionCheckReply(reply);
		return;
	}
	updateApiVersionFromReply(reply);

	// Requests answered separately: kept out of the error reporting and the generic routing below
	// (the fallback's time entries must not show up as e.g. timeEntriesReceived()).
	if (tag == TimeEntryCustomFieldsTag || tag == TimeEntryCustomFieldsFallbackTag) {
		updateConnectionStateFromReply(reply);
		handleTimeEntryCustomFieldsReply(reply, tag);
		return;
	}
	if (tag == AllProjectsTag || tag == OwnTimeEntriesTag || tag == TimeEntryReportTag) {
		updateConnectionStateFromReply(reply);
		handlePagedReply(reply);
		return;
	}
	if (tag == PermissionsTag) {
		updateConnectionStateFromReply(reply);
		handlePermissionsReply(reply);
		return;
	}
	if (tag == IssuesBatchTag) {
		handleIssuesBatchReply(reply);
		return;
	}

	if (reply->error() != QNetworkReply::NoError) {
		QByteArray data = reply->readAll();
		QJsonDocument doc = QJsonDocument::fromJson(data);
		QJsonObject response = doc.object();

		if (shouldReport(reply, reportingOf(reply))) {
			reportError(reply, response);
		}
		const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		// Only Redmine's own answer (JSON) says the issue is unknown; a 404 page from something else
		// (a wrong server URL, a proxy) says nothing about the issue.
		const QVariant issueId = reply->request().attribute(ISSUE_ID_ATTRIBUTE);
		if (issueId.isValid() && httpStatus == 404 &&
			reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("application/json")) {
			emit issueNotFound(issueId.toInt());
		}
		// A time entry that's gone on Redmine already is as good as deleted.
		if (reply->operation() == QNetworkAccessManager::DeleteOperation && httpStatus == 404 &&
			reply->url().path().contains("/time_entries/")) {
			emit timeEntryDeleted(resourceIdFromPath(reply->url().path()));
		}
		if (reply->operation() == QNetworkAccessManager::PostOperation &&
			reply->url().path().endsWith(ApiResources::TIME_ENTRIES)) {
			emit timeEntryCreateFailed(reply->request().attribute(REQUEST_TAG_ATTRIBUTE).toInt(), httpStatus,
				userErrorMessage(reply, response), retryAfterFromReply(reply));
		}

		// What Redmine answered, to find out why it refused; only logged in the content category as it
		// may hold what the user entered
		QR_DEBUG(lcRedmineContent, QStringLiteral("Error response of request #%1").arg(requestId(reply)),
			{ { "id", requestId(reply) }, { "body", QString::fromUtf8(data.left(MAX_LOGGED_BODY_SIZE)) } });

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
			if (response.contains("issue")) {
				emit issueReceived(response["issue"].toObject());
			}
			if (response.contains("issues")) {
				emit issuesReceived(response["issues"].toArray());
			}
			if (response.contains("project")) {
				emit projectReceived(response["project"].toObject());
			}
			if (response.contains("projects")) {
				emit projectsReceived(response["projects"].toArray());
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
