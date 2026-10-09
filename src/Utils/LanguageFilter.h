#ifndef LANGUAGEFILTER_H
#define LANGUAGEFILTER_H

#include <QString>
#include <QStringList>
#include <QPair>

namespace LanguageFilter {

// Fixed set: zh, en, ja, ko, other（中文不区分简繁）
inline QList<QPair<QString, QString>> allLanguageOptions() {
    return {
        {QStringLiteral("zh"),    QStringLiteral("中文")},
        {QStringLiteral("en"),    QStringLiteral("英语")},
        {QStringLiteral("ja"),    QStringLiteral("日语")},
        {QStringLiteral("ko"),    QStringLiteral("韩语")},
        {QStringLiteral("other"), QStringLiteral("其他")}
    };
}

inline QStringList allLanguageCodes() {
    QStringList codes;
    for (const auto& p : allLanguageOptions()) {
        codes << p.first;
    }
    return codes;
}

inline QString defaultLanguage() {
    return QStringLiteral("zh");
}

// Detects language; anything not clearly zh/en/ja/ko becomes "other"
QString detectLanguage(const QString& text);

// 用户名或简介任一为所选语言即通过（简介可为空）
bool passesLanguageFilter(const QString& displayName,
                          const QString& bio,
                          const QString& selectedLang,
                          QString* outNameLang = nullptr,
                          QString* outBioLang = nullptr);

// Same detection rules as JS source for injection into CEF scripts
QString jsDetectLanguageFunction();

// JSON string of selected language code, e.g. "zh-Hans"
QString buildSelectedLangJson(const QString& selectedLang);

} // namespace LanguageFilter

#endif // LANGUAGEFILTER_H
