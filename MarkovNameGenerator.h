#pragma once

#include <string>
#include <unordered_map>
#include <random>
#include <vector>

class CMarkovNameGenerator
{
public:
    CMarkovNameGenerator();
    bool Train(const std::wstring& filename, unsigned int order = 3);

    bool Save(const std::wstring& filename) const;
    bool Load(const std::wstring& filename);

    std::wstring Generate(
        const std::wstring& start = L"",
        unsigned int minLength = 4,
        unsigned int maxLength = 12);

private:
    std::unordered_map<
        std::wstring,
        std::vector<wchar_t>
    > m_model;

    unsigned int m_order = 3;

    std::mt19937 m_randomGenerator;
};
