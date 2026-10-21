// Copyright(c) 2016 YamaArashi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <string>
#include <stack>
#include <unistd.h>
#include "preproc.h"
#include "asm_file.h"
#include "c_file.h"
#include "charmap.h"
#include "translations.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static void UsageAndExit(const char *program);

Charmap* g_charmap;

void PrintAsmBytes(unsigned char *s, int length)
{
    if (length > 0)
    {
        std::printf("\t.byte ");
        for (int i = 0; i < length; i++)
        {
            std::printf("0x%02X", s[i]);

            if (i < length - 1)
                std::printf(", ");
        }
        std::putchar('\n');
    }
}

// Replacement of a script text block (a label followed by `.string` lines)
// with its translation. The English lines are read but not printed, and the
// translation is printed once the block ends: after a line ending in '$', or
// before the next line that is neither blank nor `.string`.
struct ScriptTextTranslation
{
    const std::string* text = nullptr;
    std::string label;
    std::string file;
    bool replacing = false;

    void Begin(AsmFile& file_, const Label& label_)
    {
        End();
        if (g_translations == nullptr)
            return;
        text = g_translations->LookupScriptText(file_.GetFilename(), label_.symbol);
        label = label_.symbol;
        file = file_.GetFilename();
    }

    void End()
    {
        if (replacing)
        {
            std::vector<unsigned char> bytes;
            std::string error;
            if (!EncodeTranslation(*text, bytes, error))
                FATAL_ERROR("%s: error: bad translation of %s: %s\n", file.c_str(), label.c_str(), error.c_str());
            PrintAsmBytes(bytes.data(), static_cast<int>(bytes.size()));
        }
        text = nullptr;
        replacing = false;
    }
};

void PreprocAsmFile(std::string filename, bool isStdin, bool doEnum, bool doSize)
{
    std::stack<AsmFile> stack;
    Label prevLabel;
    bool inScriptData = false;
    ScriptTextTranslation translation;

    stack.push(AsmFile(filename, isStdin, doEnum));
    std::printf("# 1 \"%s\"\n", filename.c_str());

    for (;;)
    {
        while (stack.top().IsAtEnd())
        {
            translation.End();

            const char *ps = prevLabel.symbol.c_str();
            if (doSize && inScriptData && prevLabel)
                std::printf(".ifdef %s ; .size %s, . - %s ; .endif\n", ps, ps, ps);
            prevLabel = Label();

            stack.pop();

            if (stack.empty())
                return;
            else
                stack.top().OutputLocation();
        }

        Directive directive = stack.top().GetDirective();

        switch (directive)
        {
        case Directive::Include:
            translation.End();
            stack.push(AsmFile(stack.top().ReadPath(), false, doEnum));
            stack.top().OutputLocation();
            break;
        case Directive::String:
        {
            unsigned char s[kMaxStringLength];
            if (translation.text != nullptr)
            {
                bool isPadded;
                int length = stack.top().ReadString(s, &isPadded);
                if (isPadded)
                    FATAL_ERROR("%s: error: %s has a padded .string and can't be translated\n", translation.file.c_str(), translation.label.c_str());
                translation.replacing = true;
                if (length > 0 && s[length - 1] == 0xFF)
                    translation.End();
            }
            else
            {
                int length = stack.top().ReadString(s);
                PrintAsmBytes(s, length);
            }
            break;
        }
        case Directive::Braille:
        {
            translation.End();
            unsigned char s[kMaxStringLength];
            int length = stack.top().ReadBraille(s);
            PrintAsmBytes(s, length);
            break;
        }
        case Directive::Enum:
        {
            translation.End();
            if (!stack.top().ParseEnum())
                stack.top().OutputLine();
            break;
        }
        case Directive::Macro:
        {
            translation.End();
            // GNU as misreports the filename (but not line!) when an
            // error occurs in a macro, so we work around this bug by
            // emitting an explicit location inside the macro
            // definition.
            // ... but only on the first pass, because on the second
            // pass we have lost track of our location.
            if (!isStdin)
            {
                stack.top().OutputLine();
                stack.top().OutputLocation();
            }
        }
        case Directive::Unknown:
        {
            Label label = stack.top().GetLabel();

            if (label)
            {
                const char *s = label.symbol.c_str();
                const char *ps = prevLabel.symbol.c_str();

                if (doSize && inScriptData && prevLabel)
                    std::printf(".ifdef %s ; .size %s, . - %s ; .endif ; ", ps, ps, ps);

                if (label.type == Label::global)
                    std::printf(".global %s\n%s:\n", s, s);

                if (doSize)
                    stack.top().OutputLocation();

                prevLabel = label;
                translation.Begin(stack.top(), label);
            }
            else if (translation.text != nullptr && stack.top().IsRestOfLineBlank())
            {
                stack.top().OutputLine();
            }
            else
            {
                translation.End();
                std::string section = stack.top().PeekSection();
                if (section == "script_data")
                    inScriptData = true;
                else if (section != "")
                    inScriptData = false;
                stack.top().OutputLine();
            }

            break;
        }
        }
    }
}

void PreprocCFile(const char * filename, bool isStdin, const char * graphicsRoot)
{
    CFile cFile(filename, isStdin, graphicsRoot);
    cFile.Preproc();
}

const char* GetFileExtension(const char* filename)
{
    const char* extension = filename;

    while (*extension != 0)
        extension++;

    while (extension > filename && *extension != '.')
        extension--;

    if (extension == filename)
        return nullptr;

    extension++;

    if (*extension == 0)
        return nullptr;

    return extension;
}

static void UsageAndExit(const char *program)
{
    std::fprintf(stderr, "Usage: %s [-i] [-e] [-g PATH] [-s] [-t DICT] [-z CHARMAP] SRC_FILE CHARMAP_FILE\nwhere -i denotes if input is from stdin\n      -e enables enum handling\n      -g specifies the root for INCGFX\n      -s enables '.size' handling\n      -t replaces text with the translations in DICT\n      -z adds a charmap that takes priority in translated text\n", program);
    std::exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int opt;
    const char *source = NULL;
    const char *charmap = NULL;
    bool isStdin = false;
    bool doEnum = false;
    const char *graphicsRoot = "";
    bool doSize = false;
    const char *translationsFile = NULL;
    const char *translationCharmapFile = NULL;

    /* preproc [-i] [-e] [-s] SRC_FILE CHARMAP_FILE */
    while ((opt = getopt(argc, argv, "ieg:st:z:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            isStdin = true;
            break;
        case 'e':
            doEnum = true;
            break;
        case 's':
            doSize = true;
            break;
        case 'g':
            graphicsRoot = optarg;
            break;
        case 't':
            translationsFile = optarg;
            break;
        case 'z':
            translationCharmapFile = optarg;
            break;
        default:
            UsageAndExit(argv[0]);
            break;
        }
    }

    if (optind + 2 != argc)
        UsageAndExit(argv[0]);

    source = argv[optind + 0];
    charmap = argv[optind + 1];

    g_charmap = new Charmap(charmap);
    if (translationCharmapFile != NULL)
        g_translationCharmap = new Charmap(translationCharmapFile);
    if (translationsFile != NULL)
        g_translations = new Translations(translationsFile);

#ifdef _WIN32
	// On Windows, piping from stdout can break newlines. Treat stdout as binary stream to avoid this.
	_setmode(_fileno(stdout), _O_BINARY);
#endif

    const char* extension = GetFileExtension(source);

    if (!extension)
        FATAL_ERROR("\"%s\" has no file extension.\n", argv[1]);

    if ((extension[0] == 's') && extension[1] == 0)
    {
        PreprocAsmFile(source, isStdin, doEnum, doSize);
    }
    else if ((extension[0] == 'c' || extension[0] == 'i') && extension[1] == 0)
    {
        if (doEnum)
            FATAL_ERROR("-e is invalid for C sources\n");
        PreprocCFile(source, isStdin, graphicsRoot);
    }
    else
    {
        FATAL_ERROR("\"%s\" has an unknown file extension of \"%s\".\n", argv[1], extension);
    }

    return 0;
}
