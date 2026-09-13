#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

namespace QRedmineTesting {

/*!
 * \brief The MockHttpResponse struct A canned HTTP response, to be served in enqueue order by
 * MockHttpServer.
 */
struct MockHttpResponse {
	int statusCode = 200;
	QByteArray statusText = "OK";
	QByteArray body { "{}" };
	// Header names/values as sent on the wire; Content-Length and Connection are added automatically.
	QHash<QByteArray, QByteArray> headers { { "Content-Type", "application/json" } };
};

/*!
 * \brief The RecordedHttpRequest struct A single HTTP request, as received and parsed by
 * MockHttpServer.
 */
struct RecordedHttpRequest {
	QString method;
	QString path;
	QUrlQuery query;
	QHash<QByteArray, QByteArray> headers; // header names are stored lower-cased
	QByteArray body;

	/*! \brief header Case-insensitive header lookup. */
	QByteArray header(const QByteArray& name) const { return headers.value(name.toLower()); }
};

/*!
 * \brief MockHttpServer A minimal loopback HTTP/1.1 server used to integration-test RedmineClient
 * without a real Redmine instance. Serves one canned MockHttpResponse per request received, in
 * the order they were enqueued (or a default 200 "{}" response when the queue is empty), and
 * records every request it handles for later assertions.
 */
class MockHttpServer : public QObject {
	Q_OBJECT
public:
	explicit MockHttpServer(QObject* parent = nullptr);

	/*! \brief baseUrl A "http://127.0.0.1:<port>/" URL suitable for RedmineClient::setBaseUrl. */
	QString baseUrl() const;

	quint16 port() const;

	/*! \brief enqueueResponse Queues a response to be sent for the next request received. */
	void enqueueResponse(const MockHttpResponse& response);

	/*! \brief requests All requests recorded so far, in the order they were received. */
	const QList<RecordedHttpRequest>& requests() const { return mRequests; }

signals:
	/*! \brief requestReceived Emitted once a full request has been parsed and responded to. */
	void requestReceived(const RecordedHttpRequest& request);

private slots:
	void onNewConnection();
	void onReadyRead();

private:
	void tryParseAndRespond(QTcpSocket* socket);

	QTcpServer mServer;
	QQueue<MockHttpResponse> mResponses;
	QList<RecordedHttpRequest> mRequests;
	QHash<QTcpSocket*, QByteArray> mBuffers;
};

}
