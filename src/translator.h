#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonValue>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

// Follows the language picked under Omarchy's Setup > Language. Catalogs are
// flat JSON objects from English source text to translation, one per Omarchy
// language code (i18n/da.json, i18n/bg.json), so QML keeps plain qsTr("Save").
// Adapted from Omacut (MIT, David Heinemeier Hansson).
class Translator : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(bool followingSystem READ followingSystem NOTIFY changed)

public:
    explicit Translator(const QString &languageFile = {}, const QString &catalogDirectory = ":/i18n",
                        QObject *parent = nullptr);

    QString language() const { return m_language; }
    bool isEmpty() const override { return m_strings.isEmpty(); }
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;

    // An explicit choice from the app's own UI wins over Omarchy's language
    // file until the user asks to follow the desktop again.
    Q_INVOKABLE void setLanguage(const QString &code);
    Q_INVOKABLE void followSystem();

    bool followingSystem() const { return m_override.isEmpty(); }

    static QString readLanguage(const QString &path);
    static QString pluralCategory(const QString &language, long long count);
    static QLocale formatLocale(const QString &language);

signals:
    void changed();

private:
    void reload();

    QString m_languageFile;
    QString m_catalogDirectory;
    QString m_language;
    QString m_override;
    QHash<QString, QJsonValue> m_strings;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
