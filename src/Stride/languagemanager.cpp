#include "languagemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QLoggingCategory>
#include <QVariantMap>

#include "qsettingshelper.h"
#include "redmineclient.h"

Q_LOGGING_CATEGORY(lcLanguageManager, "qredmine.timetracker.language")

namespace Stride {

static const QString LANGUAGE_SETTING = QStringLiteral("ui/language");
static const QString CATALOG_NAME = QStringLiteral("Stride");
static const QString CATALOG_DIR = QStringLiteral(":/i18n");

LanguageManager::LanguageManager(QQmlEngine* engine, QObject* parent)
	: QObject(parent)
	, mEngine(engine)
	, mLanguage(QRedmineClient::Utils::GetSetting(LANGUAGE_SETTING).toString())
{
	Q_ASSERT(!sInstance);
	sInstance = this;
	installTranslators();
}

LanguageManager::~LanguageManager() { sInstance = nullptr; }

LanguageManager* LanguageManager::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
	Q_UNUSED(jsEngine)
	Q_ASSERT(sInstance);
	Q_ASSERT(sInstance->mEngine == qmlEngine);
	// The instance is owned by main.cpp, the engine must never delete it.
	QJSEngine::setObjectOwnership(sInstance, QJSEngine::CppOwnership);
	return sInstance;
}

QString LanguageManager::language() const { return mLanguage; }

void LanguageManager::setLanguage(const QString& language)
{
	if (mLanguage == language) {
		return;
	}
	mLanguage = language;
	QRedmineClient::Utils::SaveSetting(LANGUAGE_SETTING, mLanguage);
	installTranslators();
	if (mEngine) {
		mEngine->retranslate();
	}
	emit languageChanged();
}

QVariantList LanguageManager::availableLanguages() const
{
	QVariantList languages;
	const auto catalogs =
		QDir(CATALOG_DIR).entryList({ CATALOG_NAME + QStringLiteral("_*.qm") }, QDir::Files, QDir::Name);
	for (const auto& catalog : catalogs) {
		// "Stride_de.qm" -> "de"
		const QString code = catalog.mid(CATALOG_NAME.size() + 1).chopped(3);
		const QLocale locale(code);
		// nativeLanguageName() reports the territory-specific variant for English ("American
		// English"), use the plain language name there instead.
		QString name = locale.language() == QLocale::English ? QStringLiteral("English") : locale.nativeLanguageName();
		if (!name.isEmpty()) {
			name[0] = name[0].toUpper();
		}
		languages.append(QVariantMap { { QStringLiteral("code"), code }, { QStringLiteral("name"), name } });
	}
	return languages;
}

void LanguageManager::installTranslators()
{
	const QLocale locale = mLanguage.isEmpty() ? QLocale::system() : QLocale(mLanguage);

	QCoreApplication::removeTranslator(&mTranslator);
	// Falls back to the English source strings when no matching catalog is found, e.g. for
	// unsupported system languages.
	if (mTranslator.load(locale, CATALOG_NAME, QStringLiteral("_"), CATALOG_DIR)) {
		QCoreApplication::installTranslator(&mTranslator);
	} else {
		qCDebug(lcLanguageManager) << "No translation catalog found for" << locale.name();
	}

	// The QRedmine::Client library ships its own catalog for its user-facing strings (e.g. the
	// messages emitted via RedmineClient::errorOccurred), see src/RedmineClient/i18n/README.md.
	QRedmineClient::RedmineClient::installTranslations(locale);
}
}
