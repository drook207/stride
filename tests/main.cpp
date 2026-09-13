
#include <QCoreApplication>
#include <QSettings>
#include <QTimer>
#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
	QCoreApplication app { argc, argv };

	// RedmineClient reads/writes its baseUrl and apiKey via QSettings (see qsettingshelper.h). Use a
	// dedicated organization/application name so the test suite never reads or clobbers a developer's
	// real persisted Redmine settings, which live under "YourOrg"/"Redmine Time Tracker" (see main.cpp
	// of the RedmineTimeTracker app). Also force the INI backend so settings land in a predictable,
	// easily-cleared file/location rather than e.g. the platform registry.
	QCoreApplication::setOrganizationName("QRedmineTimeTrackerTests");
	QCoreApplication::setOrganizationDomain("qredminetimetracker.tests");
	QCoreApplication::setApplicationName("QRedmineTimeTrackerTests");
	QSettings::setDefaultFormat(QSettings::IniFormat);

	QTimer::singleShot(0, [&]() {
		::testing::InitGoogleTest(&argc, argv);
		auto testResult = RUN_ALL_TESTS();
		app.exit(testResult);
	});

	return app.exec();
}
