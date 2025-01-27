// main.cpp
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);

	// Set application information for QSettings
	app.setOrganizationName("YourOrg");
	app.setOrganizationDomain("yourorg.com");
	app.setApplicationName("RedmineTimeTracker");

	// Create QML engine and load main QML file
	QQmlApplicationEngine engine;

	// Load main QML file

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);
	engine.addImportPath("drook207.redmineClient");
	engine.addImportPath("drook207.timeTracker");
	engine.loadFromModule("drook207.timeTracker", "Main");

	return app.exec();
}
