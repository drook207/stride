#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace Stride {
/*!
 * \brief The LanguageManager class selects the UI language at runtime
 *
 * Owns the application's translator and (via RedmineClient::installTranslations()) the
 * QRedmine::Client library's one, persists the selected language in QSettings and retranslates the
 * QML engine whenever it changes, so the UI switches language immediately without a restart.
 *
 * Exposed to QML as a singleton backed by the single instance created in main.cpp, which must exist
 * before the QML engine first accesses the singleton (see create()).
 */
class LanguageManager : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON
	Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged FINAL)
	Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT FINAL)
public:
	/*!
	 * \brief LanguageManager Installs the translators for the persisted language (or the system
	 * locale if none has been chosen yet)
	 * \param engine The engine to retranslate on language changes
	 * \param parent
	 */
	explicit LanguageManager(QQmlEngine* engine, QObject* parent = nullptr);
	~LanguageManager() override;

	/*!
	 * \brief create Factory used by the QML engine for the singleton, returns the instance created in
	 * main.cpp instead of constructing a new one
	 */
	static LanguageManager* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

	/*!
	 * \brief language The selected language as ISO 639-1 code (e.g. "de"), or an empty string for
	 * following the system locale
	 * \return
	 */
	QString language() const;

	/*!
	 * \brief setLanguage Switches the UI to \a language and persists the choice
	 * \param language ISO 639-1 code of one of availableLanguages(), or an empty string for the
	 * system locale
	 */
	void setLanguage(const QString& language);

	/*!
	 * \brief availableLanguages Every language a translation catalog is embedded for, as list of
	 * {"code": "de", "name": "Deutsch"} maps, the name in the language itself
	 * \return
	 */
	QVariantList availableLanguages() const;

signals:
	void languageChanged();

private:
	void installTranslators();

	static inline LanguageManager* sInstance = nullptr;

	QQmlEngine* mEngine;
	QTranslator mTranslator;
	QString mLanguage;
};
}
