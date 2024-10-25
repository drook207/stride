
#include "client.h"
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	QCommandLineParser parser;
	parser.setApplicationDescription("Test helper");
	parser.addHelpOption();
	parser.addVersionOption();
	// clang-format off
	QCommandLineOption redmineApiKey(QStringList() << "k" << "key", QCoreApplication::translate("main", "Redmine REST Api Key"));
	// clang-format on
	parser.addOption(redmineApiKey);

	parser.process(app);

	QString apiKey = QStringLiteral("ddfcf7b8579e16576afc329f0eef951817476160"); // parser.value(redmineApiKey);

	QRedmine::Client* client = new QRedmine::Client(QUrl("http://localhost:8080"), apiKey, false, &app);

	client->reconnect();

	QRedmine::TimeEntryRequest request;

	client->timeEntry(request, QRedmine::DEFAULT_ID);

	QObject::connect(&request, &QRedmine::TimeEntryRequest::finished, &app, []() { qDebug() << "Request finished!"; });

	return app.exec();
}
