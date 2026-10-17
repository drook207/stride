// main.cpp
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QSortFilterProxyModel>

#include "languagemanager.h"

// Settings were kept under the placeholder organization "YourOrg" before. Copies them over once, so
// e.g. the server URL survives the change, and removes the old store.
static void migrateLegacySettings()
{
	QSettings legacy(QStringLiteral("YourOrg"), QCoreApplication::applicationName());
	QSettings current;
	if (legacy.allKeys().isEmpty() || !current.allKeys().isEmpty()) {
		return;
	}
	for (const QString& key : legacy.allKeys()) {
		current.setValue(key, legacy.value(key));
	}
	current.sync();
	legacy.clear();
	legacy.sync();
	QFile::remove(legacy.fileName());
	QDir().rmdir(QFileInfo(legacy.fileName()).absolutePath()); // only if nothing else is left in it
}

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);

	// Application identity for QSettings and the data directory, see CMakeLists.txt.
	app.setOrganizationName(APP_ORGANIZATION_NAME);
	app.setOrganizationDomain(APP_ORGANIZATION_DOMAIN);
	app.setApplicationName(APP_NAME);
	app.setApplicationVersion(APP_VERSION);
	migrateLegacySettings();

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
