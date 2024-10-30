
#include "client/client.h"
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QString>

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

	QRedmine::Client client(QUrl("http://localhost:8080"), QStringView(apiKey), false);

	client.reconnect();

	QRedmine::Types::TimeEntry timeEntryRecord {};
	timeEntryRecord.project.name = QLatin1String("redmine-rest-client-test");
	timeEntryRecord.project.id = 1;
	timeEntryRecord.hours = 1.5;
	timeEntryRecord.spentOn = QDate(2024, 10, 27);

	QRedmine::Requests::Json::TimeEntryRequestJson request(timeEntryRecord);

	client.setTimeEntry(request, QRedmine::Global::DEFAULT_ID);

	QObject::connect(&request, &QRedmine::Requests::Json::TimeEntryRequestJson::finished, &app,
		[]() { qDebug() << "Request finished!"; });

	return QCoreApplication::exec();
}
