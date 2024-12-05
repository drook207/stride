
#include "client/client.h"
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QRestReply>
#include <QString>
#include <qlogging.h>
#include <qnetworkreply.h>
#include <qrestaccessmanager.h>

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	QCommandLineParser parser;
	parser.setApplicationDescription("Test helper");
	parser.addHelpOption();
	parser.addVersionOption();
	// clang-format off
	const QCommandLineOption redmineApiKey(QStringList() << "k" << "key", QCoreApplication::translate("main", "Redmine REST Api Key"));
	// clang-format on
	parser.addOption(redmineApiKey);

	parser.process(app);

	const QString apiKey = QLatin1String("ddfcf7b8579e16576afc329f0eef951817476160"); // parser.value(redmineApiKey);

	QRedmine::RestClient client(QUrl("http://localhost:8080"), QStringView(apiKey), false);

	if (!client.getTimeEntries([](QRestReply& reply) {
			if (reply.isSuccess()) {
				qDebug() << reply.readText();
			}
			if (reply.hasError()) {
				qDebug() << "Reply has errors: " << reply.errorString();
			}
		})) {
		qWarning() << "Failed to create request for time entry";
	}

	return QCoreApplication::exec();
}
