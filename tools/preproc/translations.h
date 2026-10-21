#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

#include <string>
#include <map>
#include <utility>
#include <vector>
#include "charmap.h"

// Translated text for localized builds (see docs/fusion/CHINESE.md).
//
// The dictionary is a UTF-8 file with one entry per line:
//     KIND <TAB> CONTEXT <TAB> KEY <TAB> TRANSLATION
// KIND is 'c' for C string literals (KEY = the English literal as written in
// the source, CONTEXT = "" or the source file it applies to) and 's' for
// script text blocks (KEY = the block's label, CONTEXT = "" or the file).
// Translations use the same syntax as the source strings.
class Translations
{
public:
    explicit Translations(const std::string& filename);

    // Returns nullptr when there is no translation.
    const std::string* LookupCString(const std::string& file, const std::string& literal) const;
    const std::string* LookupScriptText(const std::string& file, const std::string& label) const;

private:
    std::map<std::pair<std::string, std::string>, std::string> m_cStrings;
    std::map<std::pair<std::string, std::string>, std::string> m_scriptTexts;
};

extern Translations* g_translations;
extern Charmap* g_translationCharmap;

// Encodes a translated string (contents without quotes) into dest.
// Characters are looked up in g_translationCharmap first, then g_charmap.
// On failure, returns false and stores a message in error.
bool EncodeTranslation(const std::string& text, std::vector<unsigned char>& dest, std::string& error);

#endif // TRANSLATIONS_H
