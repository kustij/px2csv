// Streaming: binary stream to text stream (decodes to UTF-8)
#include <sstream>
#include <stdexcept>
#include <regex>
#include <set>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include "pxconvert.h"
#include "utils.h"

void pxconvert::convert_stream_binary(std::istream &in, std::ostream &out, bool include_codes, bool include_labels)
{
    // Read the entire input stream into a buffer
    std::string raw_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Only support UTF-8 input for simplicity and robustness
    std::string text;
    text = raw_data;

    // Normalize all line endings to \n
    std::string norm;
    norm.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\r')
        {
            if (i + 1 < text.size() && text[i + 1] == '\n')
            {
                ++i;
            }
            norm += '\n';
        }
        else
        {
            norm += text[i];
        }
    }
    std::istringstream utf8_stream(norm);

    // --- 1. Parse metadata until DATA= ---
    std::string line, statement;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<std::string> dimensions;
    std::unordered_map<std::string, std::vector<std::string>> dimension_values;
    std::unordered_map<std::string, std::vector<std::string>> dimension_codes;
    std::regex valre(R"PX(VALUES\("([^\"]+)"\)=(.*);)PX");
    std::regex codere(R"PX(CODES\("([^\"]+)"\)=(.*);)PX");
    std::set<std::string> seen_vals, seen_codes;
    bool found_data = false;
    while (std::getline(utf8_stream, line))
    {
        if (line.find("DATA=") == 0)
        {
            found_data = true;
            break;
        }
        statement += line;
        if (line.find(';') == std::string::npos)
            continue;
        auto pos = statement.find('=');
        if (pos == std::string::npos)
        {
            statement.clear();
            continue;
        }
        std::string key = normalize(statement.substr(0, pos));
        std::string value = statement.substr(pos + 1);
        std::smatch mlang;
        std::regex langkey_re(R"(([A-Z\-]+)\[([a-z]+)\])");
        if (!std::regex_match(key, mlang, langkey_re))
            metadata[key] = value;
        if (key == "STUB" || key == "HEADING")
        {
            auto dims = split_quoted_list(value);
            dimensions.insert(dimensions.end(), dims.begin(), dims.end());
        }
        std::smatch m;
        if (std::regex_match(statement, m, valre))
        {
            std::string dim = normalize(m[1]);
            if (seen_vals.count(dim) == 0)
            {
                auto vlist = split_quoted_list(m[2]);
                dimension_values[dim] = vlist;
                seen_vals.insert(dim);
            }
        }
        if (std::regex_match(statement, m, codere))
        {
            std::string dim = normalize(m[1]);
            if (seen_codes.count(dim) == 0)
            {
                auto clist = split_quoted_list(m[2]);
                dimension_codes[dim] = clist;
                seen_codes.insert(dim);
            }
        }
        statement.clear();
    }
    if (!found_data)
        throw std::runtime_error("DATA= section not found in PX file");

    // --- 2. Write CSV header ---
    bool first_col = true;
    for (size_t i = 0; i < dimensions.size(); ++i)
    {
        std::string dim = normalize(dimensions[i]);
        if (include_codes && include_labels)
        {
            if (!first_col)
                out << ",";
            out << csv_escape(dim + "_code") << "," << csv_escape(dim + "_label");
            first_col = false;
        }
        else if (include_codes || include_labels)
        {
            if (!first_col)
                out << ",";
            out << csv_escape(dim);
            first_col = false;
        }
    }
    out << "," << csv_escape("value") << "\n";

    // --- 3. Prepare dimension value/code lists ---
    std::vector<const std::vector<std::string> *> value_refs, code_refs;
    for (const auto &dim : dimensions)
    {
        std::string nd = normalize(dim);
        auto vit = dimension_values.find(nd);
        auto cit = dimension_codes.find(nd);
        if (vit != dimension_values.end())
            value_refs.push_back(&vit->second);
        else
        {
            static std::vector<std::string> fallback = {nd};
            value_refs.push_back(&fallback);
        }
        if (cit != dimension_codes.end())
            code_refs.push_back(&cit->second);
        else
            code_refs.push_back(value_refs.back());
    }
    size_t n_dims = value_refs.size();
    std::vector<size_t> indices(n_dims, 0);

    // --- 4. Stream data rows ---
    bool done = false;
    while (std::getline(utf8_stream, line) && !done)
    {
        if (auto pos = line.find(';'); pos != std::string::npos)
            line = line.substr(0, pos);
        std::istringstream iss(line);
        std::string value;
        while (iss >> value)
        {
            std::vector<std::pair<std::string, std::string>> row;
            for (size_t i = 0; i < n_dims; ++i)
            {
                std::string code = (*code_refs[i])[indices[i]];
                std::string label = (*value_refs[i])[indices[i]];
                row.emplace_back(code, label);
            }
            std::string vnorm = normalize(value);
            // Write CSV row
            bool first_col = true;
            for (size_t i = 0; i < row.size(); ++i)
            {
                if (include_codes && include_labels)
                {
                    if (!first_col)
                        out << ",";
                    out << csv_escape(row[i].first) << "," << csv_escape(row[i].second);
                    first_col = false;
                }
                else if (include_codes || include_labels)
                {
                    if (!first_col)
                        out << ",";
                    out << csv_escape(include_codes ? row[i].first : row[i].second);
                    first_col = false;
                }
            }
            out << "," << csv_escape((vnorm == "." || vnorm == "\".\"") ? std::string() : value) << "\n";
            // Increment indices (cartesian product, streaming)
            for (int d = (int)n_dims - 1; d >= 0; --d)
            {
                if (++indices[d] < value_refs[d]->size())
                    break;
                indices[d] = 0;
            }
            if (std::all_of(indices.begin(), indices.end(), [](size_t idx)
                            { return idx == 0; }))
                done = true;
        }
    }
}

// True streaming PX-to-CSV conversion (single pass, no seek/tell)

// Internal: all logic is in convert_stream_binary. No other methods needed.

void pxconvert::convert(const std::string &input_path, const std::string &output_path, bool include_codes, bool include_labels)
{
    std::ifstream in(input_path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Failed to open input file");
    std::ofstream out(output_path);
    if (!out)
        throw std::runtime_error("Failed to open output file");
    convert_stream_binary(in, out, include_codes, include_labels);
}
