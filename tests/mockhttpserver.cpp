#include "mockhttpserver.h"

#include <QHostAddress>
#include <QUrl>

namespace QRedmineTesting {

MockHttpServer::MockHttpServer(QObject* parent)
	: QObject(parent)
{
	connect(&mServer, &QTcpServer::newConnection, this, &MockHttpServer::onNewConnection);
	mServer.listen(QHostAddress::LocalHost, 0);
}

QString MockHttpServer::baseUrl() const { return QStringLiteral("http://127.0.0.1:%0/").arg(port()); }

quint16 MockHttpServer::port() const { return mServer.serverPort(); }

void MockHttpServer::enqueueResponse(const MockHttpResponse& response) { mResponses.enqueue(response); }

void MockHttpServer::onNewConnection()
{
	while (mServer.hasPendingConnections()) {
		QTcpSocket* socket = mServer.nextPendingConnection();
		mBuffers.insert(socket, QByteArray());
		connect(socket, &QTcpSocket::readyRead, this, &MockHttpServer::onReadyRead);
		connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
		connect(socket, &QObject::destroyed, this, [this, socket]() { mBuffers.remove(socket); });
	}
}

void MockHttpServer::onReadyRead()
{
	auto* socket = qobject_cast<QTcpSocket*>(sender());
	if (!socket || !mBuffers.contains(socket)) {
		return;
	}

	mBuffers[socket] += socket->readAll();
	tryParseAndRespond(socket);
}

void MockHttpServer::tryParseAndRespond(QTcpSocket* socket)
{
	QByteArray& buffer = mBuffers[socket];

	int headerEnd = buffer.indexOf("\r\n\r\n");
	if (headerEnd < 0) {
		return; // Wait for the rest of the headers to arrive.
	}

	QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
	for (QByteArray& line : lines) {
		if (line.endsWith('\r')) {
			line.chop(1);
		}
	}
	if (lines.isEmpty()) {
		return;
	}

	QByteArray requestLine = lines.takeFirst();
	QList<QByteArray> requestParts = requestLine.split(' ');
	if (requestParts.size() < 2) {
		socket->disconnectFromHost();
		return;
	}

	QHash<QByteArray, QByteArray> headers;
	for (const QByteArray& line : std::as_const(lines)) {
		int separator = line.indexOf(':');
		if (separator < 0) {
			continue;
		}
		headers.insert(line.left(separator).trimmed().toLower(), line.mid(separator + 1).trimmed());
	}

	int contentLength = headers.value("content-length").toInt();
	int bodyStart = headerEnd + 4;
	if (buffer.size() - bodyStart < contentLength) {
		return; // Wait for the rest of the body to arrive.
	}

	QByteArray body = buffer.mid(bodyStart, contentLength);
	buffer.remove(0, bodyStart + contentLength);

	// Parse the request-target (path + query) using QUrl by giving it a throwaway authority.
	QUrl url(QStringLiteral("http://mock-http-server-authority") + QString::fromUtf8(requestParts.at(1)));

	RecordedHttpRequest request;
	request.method = QString::fromLatin1(requestParts.at(0));
	request.path = url.path();
	request.query = QUrlQuery(url.query());
	request.headers = headers;
	request.body = body;
	mRequests.append(request);

	MockHttpResponse response = mResponses.isEmpty() ? MockHttpResponse {} : mResponses.dequeue();

	QByteArray responseBytes;
	responseBytes += "HTTP/1.1 " + QByteArray::number(response.statusCode) + " " + response.statusText + "\r\n";
	for (auto it = response.headers.constBegin(); it != response.headers.constEnd(); ++it) {
		responseBytes += it.key() + ": " + it.value() + "\r\n";
	}
	responseBytes += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
	responseBytes += "Connection: close\r\n\r\n";
	responseBytes += response.body;

	socket->write(responseBytes);
	socket->flush();
	socket->disconnectFromHost();

	emit requestReceived(request);
}

}
