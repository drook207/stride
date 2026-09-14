// main.cpp
#include <QDirIterator>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSortFilterProxyModel>
#include <QTranslator>

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);

	// Set application information for QSettings
	// TODO Get this information from cmake?
	app.setOrganizationName("YourOrg");
	app.setOrganizationDomain("yourorg.com");
	app.setApplicationName("Stride");
	app.setApplicationVersion("0.0.1");

	// Load the UI translation matching the system locale (see i18n/README.md). Falls back to the
	// English source strings when no matching catalog is found, e.g. for unsupported languages.
	QTranslator translator;
	if (translator.load(QLocale::system(), "QRedmineTimeTracker", "_", ":/i18n")) {
		app.installTranslator(&translator);
	}

	// Create QML engine and load main QML file
	QQmlApplicationEngine engine;

	// Load main QML file

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);

	engine.loadFromModule("QRedmine.TimeTracker", "Main");

	return app.exec();
}
