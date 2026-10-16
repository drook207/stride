// main.cpp
#include <QDirIterator>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSortFilterProxyModel>

#include "languagemanager.h"

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);

	// Set application information for QSettings
	// TODO Get this information from cmake?
	app.setOrganizationName("YourOrg");
	app.setOrganizationDomain("yourorg.com");
	app.setApplicationName("Stride");
	app.setApplicationVersion("0.0.1");

	// Window/taskbar icon, embedded via the QML module's RESOURCES. Most Wayland compositors ignore it
	// and look the icon up from the desktop entry matching the app ID instead, so the desktop file
	// name must match the entry the installer creates (see packaging/ifw/installscript.qs).
	app.setWindowIcon(QIcon(":/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/app-icon.svg"));
	app.setDesktopFileName("qredminetimetracker");

	// Create QML engine and load main QML file
	QQmlApplicationEngine engine;

	// Installs the translators for the language selected in the settings (or the system locale),
	// before any QML is loaded. Exposed to QML as the LanguageManager singleton; parented to the
	// engine so it outlives every QML object that may still reference it during teardown.
	new QRedmineClient::LanguageManager(&engine, &engine);

	// Load main QML file

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);

	engine.loadFromModule("QRedmine.TimeTracker", "Main");

	return app.exec();
}
