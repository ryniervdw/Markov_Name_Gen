
#include "MarkovNameGenerator.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <utility>

namespace
{
    constexpr wchar_t START_MARKER = L'^';
    constexpr wchar_t END_MARKER   = L'~';

    constexpr char FILE_MAGIC[] = "MKVNAME1";
    constexpr std::uint32_t FILE_VERSION = 1;

    // Rejects a sequence repeated three times consecutively.
    // Examples: "aaa", "ananana", "barbarbar".
    bool HasRepeatedPattern(const std::wstring& name)
    {
        for (size_t patternLength = 1; patternLength <= 3; ++patternLength)
        {
            const size_t requiredLength = patternLength * 3;

            if (name.size() < requiredLength)
                continue;

            for (size_t i = 0; i <= name.size() - requiredLength; ++i)
            {
                const auto first = name.begin() + i;
                const auto second = first + patternLength;
                const auto third = second + patternLength;

                if (std::equal(first, second, second) &&
                    std::equal(first, second, third))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool IsAcceptableName(const std::wstring& name)
    {
        if (name.empty())
            return false;

        return !HasRepeatedPattern(name);
    }

    bool ReadString(std::ifstream& file, std::wstring& value)
    {
        std::uint32_t length = 0;

        if (!file.read(reinterpret_cast<char*>(&length), sizeof(length)))
            return false;

        if (length > 1024)
            return false;

        value.resize(length);

        if (length != 0 &&
            !file.read(
                reinterpret_cast<char*>(value.data()),
                length * sizeof(wchar_t)))
        {
            return false;
        }

        return true;
    }

    bool WriteString(std::ofstream& file, const std::wstring& value)
    {
        if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
            return false;

        const auto length = static_cast<std::uint32_t>(value.size());

        if (!file.write(
                reinterpret_cast<const char*>(&length),
                sizeof(length)))
        {
            return false;
        }

        if (length != 0 &&
            !file.write(
                reinterpret_cast<const char*>(value.data()),
                length * sizeof(wchar_t)))
        {
            return false;
        }

        return true;
    }

    bool DecodeInput(const std::string& input, std::wstring& output)
    {
        output.clear();

        if (input.empty())
            return true;

        // Remove UTF-8 BOM if present.
        size_t offset = 0;

        if (input.size() >= 3 &&
            static_cast<unsigned char>(input[0]) == 0xEF &&
            static_cast<unsigned char>(input[1]) == 0xBB &&
            static_cast<unsigned char>(input[2]) == 0xBF)
        {
            offset = 3;
        }

        const char* data = input.data() + offset;
        const int length = static_cast<int>(input.size() - offset);

        if (length == 0)
            return true;

        // Try UTF-8 first.
        int required = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            data,
            length,
            nullptr,
            0);

        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;

        // Fall back to the Windows ANSI code page.
        if (required == 0)
        {
            codePage = CP_ACP;
            flags = 0;

            required = MultiByteToWideChar(
                codePage,
                flags,
                data,
                length,
                nullptr,
                0);
        }

        if (required == 0)
            return false;

        output.resize(required);

        return MultiByteToWideChar(
            codePage,
            flags,
            data,
            length,
            output.data(),
            required) == required;
    }

    std::wstring Trim(const std::wstring& value)
    {
        const auto first = value.find_first_not_of(L" \t\v\f");

        if (first == std::wstring::npos)
            return {};

        const auto last = value.find_last_not_of(L" \t\v\f");

        return value.substr(first, last - first + 1);
    }

    void FormatName(std::wstring& name)
    {
        if (name.empty())
            return;

        name[0] = static_cast<wchar_t>(std::towupper(name[0]));

        for (size_t i = 1; i < name.size(); ++i)
        {
            name[i] = static_cast<wchar_t>(std::towlower(name[i]));
        }
    }
}

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------

CMarkovNameGenerator::CMarkovNameGenerator()
{
    // Seed once per generator instance.
    // This applies whether the model is trained or loaded from disk.
    std::random_device rd;

    std::seed_seq seed
    {
        rd(), rd(), rd(), rd(),
        rd(), rd(), rd(), rd()
    };

    m_randomGenerator.seed(seed);
}

// -----------------------------------------------------------------------------
// Train
// -----------------------------------------------------------------------------

bool CMarkovNameGenerator::Train(
    const std::wstring& filename,
    unsigned int order)
{
    if (order == 0 || order > 32)
        return false;

    std::ifstream file(filename, std::ios::binary);

    if (!file)
        return false;

    const std::string input{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>{}
    };

    std::wstring contents;

    if (!DecodeInput(input, contents))
        return false;

    std::unordered_map<std::wstring, std::vector<wchar_t>> newModel;

    auto AddName = [&](const std::wstring& rawName)
    {
        std::wstring name = Trim(rawName);

        if (name.empty())
            return;

        std::wstring context(order, START_MARKER);

        for (wchar_t ch : name)
        {
            newModel[context].push_back(ch);

            context.erase(context.begin());
            context.push_back(ch);
        }

        newModel[context].push_back(END_MARKER);
    };

    // Support CR, LF and CRLF line endings.
    std::wstring currentName;

    for (wchar_t ch : contents)
    {
        if (ch == L'\r' || ch == L'\n')
        {
            AddName(currentName);
            currentName.clear();
        }
        else
        {
            currentName.push_back(ch);
        }
    }

    // Process final line if the file doesn't end with a newline.
    AddName(currentName);

    if (newModel.empty())
        return false;

    m_model.swap(newModel);
    m_order = order;

    return true;
}

// -----------------------------------------------------------------------------
// Save
// -----------------------------------------------------------------------------

bool CMarkovNameGenerator::Save(const std::wstring& filename) const
{
    if (m_model.empty() || m_order == 0)
        return false;

    std::ofstream file(filename, std::ios::binary);

    if (!file)
        return false;

    // File header.
    file.write(FILE_MAGIC, sizeof(FILE_MAGIC) - 1);

    const std::uint32_t version = FILE_VERSION;
    const std::uint32_t order = m_order;
    const std::uint32_t entryCount =
        static_cast<std::uint32_t>(m_model.size());

    if (!file.write(
            reinterpret_cast<const char*>(&version),
            sizeof(version)))
        return false;

    if (!file.write(
            reinterpret_cast<const char*>(&order),
            sizeof(order)))
        return false;

    if (!file.write(
            reinterpret_cast<const char*>(&entryCount),
            sizeof(entryCount)))
        return false;

    for (const auto& [context, transitions] : m_model)
    {
        if (!WriteString(file, context))
            return false;

        if (transitions.size() >
            (std::numeric_limits<std::uint32_t>::max)())
        {
            return false;
        }

        const auto transitionCount =
            static_cast<std::uint32_t>(transitions.size());

        if (!file.write(
                reinterpret_cast<const char*>(&transitionCount),
                sizeof(transitionCount)))
        {
            return false;
        }

        if (transitionCount != 0 &&
            !file.write(
                reinterpret_cast<const char*>(transitions.data()),
                transitionCount * sizeof(wchar_t)))
        {
            return false;
        }
    }

    return file.good();
}

// -----------------------------------------------------------------------------
// Load
// -----------------------------------------------------------------------------

bool CMarkovNameGenerator::Load(const std::wstring& filename)
{
    std::ifstream file(filename, std::ios::binary);

    if (!file)
        return false;

    char magic[sizeof(FILE_MAGIC) - 1] = {};

    if (!file.read(magic, sizeof(magic)))
        return false;

    if (!std::equal(
            std::begin(magic),
            std::end(magic),
            std::begin(FILE_MAGIC)))
    {
        return false;
    }

    std::uint32_t version = 0;
    std::uint32_t order = 0;
    std::uint32_t entryCount = 0;

    if (!file.read(
            reinterpret_cast<char*>(&version),
            sizeof(version)))
        return false;

    if (!file.read(
            reinterpret_cast<char*>(&order),
            sizeof(order)))
        return false;

    if (!file.read(
            reinterpret_cast<char*>(&entryCount),
            sizeof(entryCount)))
        return false;

    if (version != FILE_VERSION || order == 0 || order > 32)
        return false;

    if (entryCount == 0 || entryCount > 1000000)
        return false;

    std::unordered_map<std::wstring, std::vector<wchar_t>> newModel;

    for (std::uint32_t i = 0; i < entryCount; ++i)
    {
        std::wstring context;

        if (!ReadString(file, context))
            return false;

        if (context.size() != order)
            return false;

        std::uint32_t transitionCount = 0;

        if (!file.read(
                reinterpret_cast<char*>(&transitionCount),
                sizeof(transitionCount)))
        {
            return false;
        }

        if (transitionCount == 0 || transitionCount > 1000000)
            return false;

        std::vector<wchar_t> transitions(transitionCount);

        if (!file.read(
                reinterpret_cast<char*>(transitions.data()),
                transitionCount * sizeof(wchar_t)))
        {
            return false;
        }

        if (!newModel.emplace(
                std::move(context),
                std::move(transitions)).second)
        {
            return false;
        }
    }

    m_model.swap(newModel);
    m_order = order;

    return true;
}

// -----------------------------------------------------------------------------
// Generate
// -----------------------------------------------------------------------------

std::wstring CMarkovNameGenerator::Generate(
    const std::wstring& start,
    unsigned int minLength,
    unsigned int maxLength)
{
    if (m_model.empty() ||
        m_order == 0 ||
        minLength > maxLength ||
        maxLength == 0)
    {
        return {};
    }

    if (start.size() > maxLength)
        return {};

    constexpr unsigned int MAX_ATTEMPTS = 64;

    for (unsigned int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        std::wstring result = start;

        std::wstring context(m_order, START_MARKER);

        // If a prefix was supplied, use its last characters as the context.
        for (wchar_t ch : start)
        {
            context.erase(context.begin());
            context.push_back(ch);
        }

        bool failed = false;

        while (result.size() < maxLength)
        {
            const auto it = m_model.find(context);

            if (it == m_model.end() || it->second.empty())
            {
                failed = true;
                break;
            }

            const auto& transitions = it->second;

            std::uniform_int_distribution<size_t> distribution(
                0,
                transitions.size() - 1);

            const wchar_t next = transitions[distribution(m_randomGenerator)];

            if (next == END_MARKER)
            {
                if (result.size() >= minLength)
                {
                    FormatName(result);

                    if (IsAcceptableName(result))
                        return result;
                }

                failed = true;
                break;
            }

            result.push_back(next);

            context.erase(context.begin());
            context.push_back(next);
        }

        // Reached maximum length. Accept only if the model permits
        // the name to end at this point.
        if (!failed && result.size() == maxLength)
        {
            const auto it = m_model.find(context);

            if (it != m_model.end())
            {
                const auto& transitions = it->second;

                if (std::find(
                        transitions.begin(),
                        transitions.end(),
                        END_MARKER) != transitions.end())
                {
                    FormatName(result);

                    if (IsAcceptableName(result))
                        return result;
                }
            }
        }
    }

    return {};
}
