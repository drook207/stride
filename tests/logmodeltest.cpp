#include "logtest.h"
#include "testutils.h"

#include <QJsonDocument>
#include <QSettings>
#include <gtest/gtest.h>

#include "logging.h"
#include "logmodel.h"

using QRedmineClient::Utils::Logging::LogManager;
using QRedmineTesting::waitUntil;
using Stride::LogModel;

namespace {
Q_LOGGING_CATEGORY(lcA, "qredmine.a")
Q_LOGGING_CATEGORY(lcB, "qredmine.b")
}

/*!
 * \brief The LogModelTest class A LogManager at the debug level holding four entries (debug, info with fields,
 * warning, critical in two categories), and a LogModel on it
 */
class LogModelTest : public LogTest {
protected:
	void SetUp() override
	{
		LogTest::SetUp();
		// Persisted, so that no "level changed" entry is logged
		QSettings().setValue(LogManager::LEVEL_SETTING, QStringLiteral("debug"));
		createManager();
		QR_DEBUG(lcA, QStringLiteral("Alpha start"));
		QR_INFO(lcA, QStringLiteral("Loaded"), { { "user", QStringLiteral("Müller") } });
		QR_WARNING(lcB, QStringLiteral("Slow response"), { { "ms", 4000 } });
		qCCritical(lcB) << "Boom";
		model = std::make_unique<LogModel>();
	}

	void TearDown() override
	{
		model.reset();
		LogTest::TearDown();
	}

	QString messageAt(int row) const
	{
		return model->data(model->index(row, 0), model->roleNames().key("message")).toString();
	}

	std::unique_ptr<LogModel> model;
};

TEST_F(LogModelTest, ListsTheBufferedEntriesOldestFirst)
{
	ASSERT_EQ(model->rowCount(), 4);
	EXPECT_EQ(model->property("count").toInt(), 4);
	EXPECT_EQ(messageAt(0).toStdString(), "Alpha start");
	EXPECT_EQ(messageAt(3).toStdString(), "Boom");
}

TEST_F(LogModelTest, ExposesTheRoles)
{
	const QHash<int, QByteArray> roles = model->roleNames();
	auto value = [&](int row, const char* role) { return model->data(model->index(row, 0), roles.key(role)); };
	EXPECT_EQ(value(0, "level").toString().toStdString(), "debug");
	EXPECT_EQ(value(0, "levelValue").toInt(), 0);
	EXPECT_EQ(value(1, "level").toString().toStdString(), "info");
	EXPECT_EQ(value(1, "category").toString().toStdString(), "qredmine.a");
	EXPECT_TRUE(value(1, "ts").toDateTime().isValid());
	EXPECT_EQ(QJsonDocument::fromJson(value(1, "fields").toString().toUtf8()).object().value("user").toString(),
		QStringLiteral("Müller"));
	EXPECT_TRUE(value(0, "fields").toString().isEmpty());
	EXPECT_TRUE(value(0, "source").toString().isEmpty());
	EXPECT_TRUE(value(2, "source").toString().contains(QStringLiteral("logmodeltest.cpp:")));
	EXPECT_EQ(value(3, "levelValue").toInt(), 3);
}

TEST_F(LogModelTest, FiltersByMinimumLevel)
{
	model->setMinimumLevel(LogModel::Info);
	EXPECT_EQ(model->rowCount(), 3);
	model->setMinimumLevel(LogModel::Warning);
	EXPECT_EQ(model->rowCount(), 2);
	model->setMinimumLevel(LogModel::Critical);
	ASSERT_EQ(model->rowCount(), 1);
	EXPECT_EQ(messageAt(0).toStdString(), "Boom");
	model->setMinimumLevel(LogModel::Fatal);
	EXPECT_EQ(model->rowCount(), 0);
	model->setMinimumLevel(LogModel::Debug);
	EXPECT_EQ(model->rowCount(), 4);
}

TEST_F(LogModelTest, FiltersByCategory)
{
	model->setCategory(QStringLiteral("qredmine.b"));
	ASSERT_EQ(model->rowCount(), 2);
	EXPECT_EQ(messageAt(0).toStdString(), "Slow response");
	model->setCategory(QStringLiteral("nothing"));
	EXPECT_EQ(model->rowCount(), 0);
	model->setCategory(QString());
	EXPECT_EQ(model->rowCount(), 4);
}

TEST_F(LogModelTest, SearchesCaseInsensitivelyInMessageCategoryAndFields)
{
	model->setSearchText(QStringLiteral("SLOW"));
	ASSERT_EQ(model->rowCount(), 1);
	EXPECT_EQ(messageAt(0).toStdString(), "Slow response");

	model->setSearchText(QStringLiteral("QREDMINE.A")); // category
	EXPECT_EQ(model->rowCount(), 2);

	model->setSearchText(QStringLiteral("mÜLLER")); // a field value, with a non-ASCII letter
	ASSERT_EQ(model->rowCount(), 1);
	EXPECT_EQ(messageAt(0).toStdString(), "Loaded");

	model->setSearchText(QStringLiteral("4000")); // a number in the fields
	EXPECT_EQ(model->rowCount(), 1);

	model->setSearchText(QStringLiteral("no such text"));
	EXPECT_EQ(model->rowCount(), 0);
}

TEST_F(LogModelTest, CombinesTheFilters)
{
	model->setCategory(QStringLiteral("qredmine.b"));
	model->setMinimumLevel(LogModel::Critical);
	EXPECT_EQ(model->rowCount(), 1);
	model->setSearchText(QStringLiteral("slow"));
	EXPECT_EQ(model->rowCount(), 0);
}

TEST_F(LogModelTest, ListsTheCategoriesSorted)
{
	EXPECT_EQ(model->categories(), (QStringList { "qredmine.a", "qredmine.b" }));
	// Not narrowed by the filters
	model->setCategory(QStringLiteral("qredmine.a"));
	EXPECT_EQ(model->categories().size(), 2);
}

TEST_F(LogModelTest, NotifiesAboutCountAndFilterChanges)
{
	// A proxy only reports changes of rows it has mapped, i.e. that a view (or a binding on count) has read
	ASSERT_EQ(model->rowCount(), 4);
	int countChanges = 0;
	int levelChanges = 0;
	QObject::connect(model.get(), &LogModel::countChanged, [&] { ++countChanges; });
	QObject::connect(model.get(), &LogModel::minimumLevelChanged, [&] { ++levelChanges; });
	model->setMinimumLevel(LogModel::Warning);
	model->setMinimumLevel(LogModel::Warning); // no change
	EXPECT_EQ(levelChanges, 1);
	EXPECT_GE(countChanges, 1);
	EXPECT_EQ(model->property("count").toInt(), 2);
}

TEST_F(LogModelTest, RowAsJsonIsTheLineOfTheLogFile)
{
	model->setMinimumLevel(LogModel::Warning); // row 0 of the proxy is not row 0 of the log
	const QString json = model->rowAsJson(0);
	const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
	EXPECT_EQ(object.value("msg").toString().toStdString(), "Slow response");
	EXPECT_EQ(object.value("fields").toObject().value("ms").toInt(), 4000);
	EXPECT_FALSE(json.endsWith('\n'));
	EXPECT_EQ(object, findInLog(QStringLiteral("Slow response")));

	EXPECT_TRUE(model->rowAsJson(2).isEmpty());
	EXPECT_TRUE(model->rowAsJson(-1).isEmpty());
}

TEST_F(LogModelTest, ShowsNewMessagesOnceEach)
{
	QR_INFO(lcA, QStringLiteral("late"));
	ASSERT_TRUE(waitUntil([&] { return model->rowCount() == 5; }));
	EXPECT_EQ(messageAt(4).toStdString(), "late");
	// The queued announcements of the first four are skipped, they were in the buffer already
	QR_INFO(lcB, QStringLiteral("later"));
	ASSERT_TRUE(waitUntil([&] { return model->rowCount() == 6; }));
	EXPECT_EQ(model->rowCount(), 6);
}

TEST_F(LogModelTest, IsEmptyWithoutALogManager)
{
	model.reset();
	destroyManager();
	LogModel empty;
	EXPECT_EQ(empty.rowCount(), 0);
	EXPECT_TRUE(empty.categories().isEmpty());
}

TEST_F(LogModelTest, KeepsTheLatestEntriesOnly)
{
	for (int i = 0; i < LogManager::MAX_BUFFERED_ENTRIES + 600; ++i) {
		QR_INFO(lcA, QString::number(i));
	}
	ASSERT_TRUE(waitUntil(
		[&] { return messageAt(model->rowCount() - 1) == QString::number(LogManager::MAX_BUFFERED_ENTRIES + 599); }));
	EXPECT_LE(model->rowCount(), LogManager::MAX_BUFFERED_ENTRIES + LogManager::MAX_BUFFERED_ENTRIES / 10);
	// By value (+), see LogManagerTest
	EXPECT_GE(model->rowCount(), +LogManager::MAX_BUFFERED_ENTRIES);
}
