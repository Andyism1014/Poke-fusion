#include <cstdio>
#include <stdexcept>
#include "preproc.h"
#include "translations.h"
#include "string_parser.h"
#include "io.h"

Translations* g_translations = nullptr;
Charmap* g_translationCharmap = nullptr;

Translations::Translations(const std::string& filename)
{
    long size = 0;
    char* buffer = ReadFileToBuffer(filename.c_str(), false, &size);
    long lineNum = 1;
    long pos = 0;

    while (pos < size)
    {
        long end = pos;
        while (end < size && buffer[end] != '\n')
            end++;

        std::string line(&buffer[pos], end - pos);
        pos = end + 1;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (!line.empty())
        {
            std::string fields[4];
            std::size_t start = 0;
            for (int i = 0; i < 4; i++)
            {
                std::size_t tab = (i < 3) ? line.find('\t', start) : std::string::npos;
                if (i < 3 && tab == std::string::npos)
                    FATAL_ERROR("%s:%ld: error: expected 4 tab-separated fields\n", filename.c_str(), lineNum);
                fields[i] = line.substr(start, tab == std::string::npos ? std::string::npos : tab - start);
                start = tab + 1;
            }

            auto key = std::make_pair(fields[1], fields[2]);
            if (fields[0] == "c")
                m_cStrings[key] = fields[3];
            else if (fields[0] == "s")
                m_scriptTexts[key] = fields[3];
            else
                FATAL_ERROR("%s:%ld: error: unknown entry kind '%s'\n", filename.c_str(), lineNum, fields[0].c_str());
        }

        lineNum++;
    }

    free(buffer);
}

const std::string* Translations::LookupCString(const std::string& file, const std::string& literal) const
{
    auto it = m_cStrings.find(std::make_pair(file, literal));
    if (it == m_cStrings.end())
        it = m_cStrings.find(std::make_pair(std::string(), literal));
    return it == m_cStrings.end() ? nullptr : &it->second;
}

const std::string* Translations::LookupScriptText(const std::string& file, const std::string& label) const
{
    auto it = m_scriptTexts.find(std::make_pair(file, label));
    if (it == m_scriptTexts.end())
        it = m_scriptTexts.find(std::make_pair(std::string(), label));
    return it == m_scriptTexts.end() ? nullptr : &it->second;
}

bool EncodeTranslation(const std::string& text, std::vector<unsigned char>& dest, std::string& error)
{
    std::vector<char> buffer;
    buffer.reserve(text.size() + 3);
    buffer.push_back('"');
    buffer.insert(buffer.end(), text.begin(), text.end());
    buffer.push_back('"');
    buffer.push_back(0);

    unsigned char s[kMaxStringLength];
    int length = 0;
    StringParser parser(buffer.data(), static_cast<long>(buffer.size() - 1), true);

    try
    {
        if (parser.ParseString(0, s, length) != static_cast<int>(text.size() + 2))
        {
            error = "unescaped '\"' in translation";
            return false;
        }
    }
    catch (std::runtime_error& e)
    {
        error = e.what();
        return false;
    }

    dest.assign(s, s + length);
    return true;
}
