// main.cpp
#include "redmineclient.h"
#include "timetracker.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);

	// Set application information for QSettings
	app.setOrganizationName("YourOrg");
	app.setOrganizationDomain("yourorg.com");
	app.setApplicationName("RedmineTimeTracker");

	// Register types with QML
	qmlRegisterType<RedmineClient>("RedmineClient", 1, 0, "RedmineClient");
	qmlRegisterType<TimeTracker>("RedmineClient", 1, 0, "TimeTracker");

	// Create QML engine and load main QML file
	QQmlApplicationEngine engine;

	// Load main QML file

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);

	engine.loadFromModule("Main", "Main");

	return app.exec();
}
