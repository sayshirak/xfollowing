#include "LanguageFilter.h"
#include <QJsonArray>
#include <QJsonDocument>

namespace LanguageFilter {

namespace {

bool isHangul(QChar c) {
    const ushort u = c.unicode();
    return (u >= 0xAC00 && u <= 0xD7AF) || (u >= 0x1100 && u <= 0x11FF) || (u >= 0x3130 && u <= 0x318F);
}

bool isKana(QChar c) {
    const ushort u = c.unicode();
    return (u >= 0x3040 && u <= 0x309F) || (u >= 0x30A0 && u <= 0x30FF) || (u >= 0x31F0 && u <= 0x31FF);
}

bool isCjk(QChar c) {
    const ushort u = c.unicode();
    return (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF);
}

bool isLatinLetter(QChar c) {
    return c.isLetter() && c.script() == QChar::Script_Latin;
}

} // namespace

// 判定规则（新）：
// - 用户名：只要含任意 CJK 汉字 → 中文(zh)
// - 简介：CJK 汉字占字母类字符 >= 50% → 中文(zh)
// - 其余按韩/日/英/其他；不再区分简繁
QString detectLanguage(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("other");
    }

    int hangul = 0, kana = 0, cjk = 0, latin = 0, letters = 0;
    for (QChar c : trimmed) {
        if (!c.isLetter() && !isCjk(c) && !isHangul(c) && !isKana(c)) {
            continue;
        }
        ++letters;
        if (isHangul(c)) ++hangul;
        else if (isKana(c)) ++kana;
        else if (isCjk(c)) ++cjk;
        else if (isLatinLetter(c)) ++latin;
    }

    if (letters == 0) {
        return QStringLiteral("other");
    }

    // 中文优先：汉字过半即中文（不区分简繁）
    if (cjk * 2 >= letters) {
        return QStringLiteral("zh");
    }
    if (hangul * 2 >= letters) {
        return QStringLiteral("ko");
    }
    if (kana * 2 >= letters || (kana > 0 && kana + cjk >= letters * 2 / 3)) {
        return QStringLiteral("ja");
    }
    if (latin * 2 >= letters) {
        return QStringLiteral("en");
    }

    return QStringLiteral("other");
}

// 新规则：用户名或简介任一为所选语言即通过（中文不再区分简繁，简介可为空）
bool passesLanguageFilter(const QString& displayName,
                          const QString& bio,
                          const QString& selectedLang,
                          QString* outNameLang,
                          QString* outBioLang) {
    if (selectedLang.isEmpty()) {
        return false;
    }

    const QString nameLang = detectLanguage(displayName);
    if (outNameLang) *outNameLang = nameLang;

    const QString bioTrim = bio.trimmed();
    const QString bioLang = bioTrim.isEmpty() ? QStringLiteral("other") : detectLanguage(bioTrim);
    if (outBioLang) *outBioLang = bioLang;

    return (nameLang == selectedLang) || (bioLang == selectedLang);
}

QString buildSelectedLangJson(const QString& selectedLang) {
    // Produce a JSON string literal, e.g. "zh"
    return QString::fromUtf8(QJsonDocument(QJsonArray{selectedLang}).toJson(QJsonDocument::Compact))
               .mid(1)
               .chopped(1);
}

QString jsDetectLanguageFunction() {
    return QString::fromUtf8(
R"JS(
function xfollowDetectLanguage(text) {
    if (!text) return 'other';
    const trimmed = String(text).trim();
    if (!trimmed) return 'other';

    function isHangul(ch) {
        const u = ch.codePointAt(0);
        return (u >= 0xAC00 && u <= 0xD7AF) || (u >= 0x1100 && u <= 0x11FF) || (u >= 0x3130 && u <= 0x318F);
    }
    function isKana(ch) {
        const u = ch.codePointAt(0);
        return (u >= 0x3040 && u <= 0x309F) || (u >= 0x30A0 && u <= 0x30FF) || (u >= 0x31F0 && u <= 0x31FF);
    }
    function isCjk(ch) {
        const u = ch.codePointAt(0);
        return (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF);
    }
    function isLatinLetter(ch) {
        try { return /\p{Script=Latin}/u.test(ch); } catch (e) { return /[A-Za-z\u00C0-\u024F]/.test(ch); }
    }

    let hangul = 0, kana = 0, cjk = 0, latin = 0, letters = 0;
    for (const ch of trimmed) {
        let isLetter = false;
        try { isLetter = /\p{L}/u.test(ch); } catch (e) { isLetter = /[A-Za-z\u00C0-\u024F\u0400-\u04FF]/.test(ch); }
        if (!isLetter && !isCjk(ch) && !isHangul(ch) && !isKana(ch)) continue;
        letters++;
        if (isHangul(ch)) hangul++;
        else if (isKana(ch)) kana++;
        else if (isCjk(ch)) cjk++;
        else if (isLatinLetter(ch)) latin++;
    }
    if (letters === 0) return 'other';

    // 中文优先：汉字过半即中文（不区分简繁）
    if (cjk * 2 >= letters) return 'zh';
    if (hangul * 2 >= letters) return 'ko';
    if (kana * 2 >= letters || (kana > 0 && kana + cjk >= Math.floor(letters * 2 / 3))) return 'ja';
    if (latin * 2 >= letters) return 'en';
    return 'other';
}

// 用户名或简介任一为所选语言即通过；简介可为空
function xfollowPassesLanguageFilter(displayName, bio, selectedLang) {
    if (!selectedLang) return null;
    const nameLang = xfollowDetectLanguage(displayName);
    const bioTrim = (bio || '').trim();
    const bioLang = bioTrim ? xfollowDetectLanguage(bioTrim) : 'other';
    if (nameLang !== selectedLang && bioLang !== selectedLang) return null;
    return { nameLang: nameLang, bioLang: bioLang, bio: bioTrim };
}
)JS");
}

} // namespace LanguageFilter
