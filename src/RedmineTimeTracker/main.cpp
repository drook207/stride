// main.cpp
#include <QDirIterator>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSortFilterProxyModel>

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);

	// Set application information for QSettings
	// TODO Get this information from cmake?
	app.setOrganizationName("YourOrg");
	app.setOrganizationDomain("yourorg.com");
	app.setApplicationName("Stride");
	app.setApplicationVersion("0.0.1");

	// Create QML engine and load main QML file
	QQmlApplicationEngine engine;

	// Load main QML file

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);

	engine.loadFromModule("QRedmine.TimeTracker", "Main");

	return app.exec();
}
