// main.cpp
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QSortFilterProxyModel>

#ifdef STRIDE_WIDGETS_APPLICATION
// The tray icon's menu (Qt.labs.platform) needs a QApplication wherever it falls back to a Qt Widgets
// menu, i.e. where the platform has no native one; see CMakeLists.txt.
#include <QApplication>
using Application = QApplication;
#else
#include <QGuiApplication>
using Application = QGuiApplication;
#endif

#include "datamigration.h"
#include "languagemanager.h"
#include "logging.h"
#include "logmanager.h"

Q_LOGGING_CATEGORY(lcMigration, "qredmine.stride.migration")

// Settings were kept under the placeholder organization "YourOrg" before. Copies them over once, so
// e.g. the server URL survives the change, and removes the old store.
static void migrateLegacySettings()
{
	QSettings legacy(QStringLiteral("YourOrg"), QCoreApplication::applicationName());
	QSettings current;
	if (!Stride::DataMigration::migrateSettings(legacy, current)) {
		return;
	}
	legacy.clear();
	legacy.sync();
	QFile::remove(legacy.fileName());
	QDir().rmdir(QFileInfo(legacy.fileName()).absolutePath()); // only if nothing else is left in it
}

int main(int argc, char* argv[])
{
	Application app(argc, argv);

	// Application identity for QSettings and the data directory, see CMakeLists.txt. The organization name
	// equals the application name on purpose: QSettings then uses ~/.config/Stride/Stride.conf (Windows:
	// HKCU\Software\Stride\Stride) and Utils::appDataPath() doesn't repeat the name for the data
	// directory, ~/.local/share/Stride (Windows: %LOCALAPPDATA%\Stride). An empty organization name would
	// make QSettings use the organization domain (or "Unknown Organization") instead.
	app.setOrganizationName(APP_ORGANIZATION_NAME);
	app.setOrganizationDomain(APP_ORGANIZATION_DOMAIN);
	app.setApplicationName(APP_NAME);
	app.setApplicationVersion(APP_VERSION);

	// Brings the settings and data from before the rename, kept under "QRedmine/Stride", along (see
	// datamigration.h). Before the log file and the database are opened and anything reads the settings,
	// so those find them in the new place; what it did is logged once the log is open.
	const Stride::DataMigration::Report migration = Stride::DataMigration::migrateLegacyApplicationData();

	// Records everything logged from here on to the log file and the in-app log view. Created before
	// anything else logs, and declared before the engine so that it outlives it.
	QRedmineClient::Utils::Logging::LogManager logManager;
	logManager.logStartup();
	QObject::connect(
		&app, &QCoreApplication::aboutToQuit, &logManager, &QRedmineClient::Utils::Logging::LogManager::logShutdown);

	for (const QString& message : migration.info) {
		QR_INFO(lcMigration, message);
	}
	for (const QString& message : migration.warnings) {
		QR_WARNING(lcMigration, message);
	}

	migrateLegacySettings();

	// Window/taskbar icon, embedded via the QML module's RESOURCES. Most Wayland compositors ignore it
	// and look the icon up from the desktop entry matching the app ID instead, so the desktop file
	// name must match the entry the installer creates (see packaging/ifw/installscript.qs).
	app.setWindowIcon(QIcon(":/qt/qml/Stride/qml/Assets/Icons/app-icon.svg"));
	app.setDesktopFileName("io.github.drook207.Stride");

	// Create QML engine and load main QML file
	QQmlApplicationEngine engine;

	// Installs the translators for the language selected in the settings (or the system locale),
	// before any QML is loaded. Exposed to QML as the LanguageManager singleton; parented to the
	// engine so it outlives every QML object that may still reference it during teardown.
	new Stride::LanguageManager(&engine, &engine);

	// Load main QML file

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);

	engine.loadFromModule("Stride", "Main");

	return app.exec();
}
