#pragma once
#include <string>
#include <vector>
#include <string_view>

// Utility: trim whitespace and quotes from both ends
inline std::string_view trim_view(std::string_view s)
{
    size_t start = s.find_first_not_of(" \";");
    if (start == std::string::npos)
        return {};
    size_t end = s.find_last_not_of(" \";");
    return s.substr(start, end - start + 1);
}

// Normalize: remove whitespace/quotes from both ends
inline std::string normalize(const std::string &s)
{
    return std::string(trim_view(s));
}
inline std::string normalize(std::string_view s)
{
    return std::string(trim_view(s));
}

// Split quoted list (string_view version)
inline std::vector<std::string_view> split_quoted_list_sv(std::string_view input)
{
    std::vector<std::string_view> result;
    size_t start = 0, len = input.size();
    bool in_quotes = false;
    for (size_t i = 0; i < len; ++i)
    {
        char c = input[i];
        if (c == '"')
            in_quotes = !in_quotes;
        else if (c == ',' && !in_quotes)
        {
            auto sv = trim_view(input.substr(start, i - start));
            if (!sv.empty())
                result.push_back(sv);
            start = i + 1;
        }
    }
    auto sv = trim_view(input.substr(start));
    if (!sv.empty())
        result.push_back(sv);
    return result;
}

// Split quoted list (returns std::string)
inline std::vector<std::string> split_quoted_list(const std::string &input)
{
    std::vector<std::string> out;
    for (auto sv : split_quoted_list_sv(input))
        out.emplace_back(sv);
    return out;
}
inline std::vector<std::string> split_quoted_list(std::string_view input)
{
    std::vector<std::string> out;
    for (auto sv : split_quoted_list_sv(input))
        out.emplace_back(sv);
    return out;
}

inline std::string csv_escape(const std::string &field)
{
    if (field.find_first_of(",\n\"") != std::string::npos)
    {
        std::string quoted = "\"";
        for (char c : field)
        {
            if (c == '"')
                quoted += '"';
            quoted += c;
        }
        quoted += '"';
        return quoted;
    }
    return field;
}
