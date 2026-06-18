// Streaming: binary stream to text stream (decodes to UTF-8)
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <fstream>
#include "px2csv.h"
#include "utils.h"

namespace
{
    bool has_statement_terminator(const std::string &line)
    {
        bool in_quotes = false;
        for (char ch : line)
        {
            if (ch == '"')
                in_quotes = !in_quotes;
            else if (ch == ';' && !in_quotes)
                return true;
        }
        return false;
    }

    bool parse_dimension_list_statement(const std::string &statement, const std::string &keyword, std::string &dimension, std::vector<std::string> &items)
    {
        const std::string prefix = keyword + "(\"";
        if (statement.rfind(prefix, 0) != 0)
            return false;

        const size_t dimension_start = prefix.size();
        const size_t dimension_end = statement.find("\")=", dimension_start);
        if (dimension_end == std::string::npos)
            return false;

        dimension = normalize(statement.substr(dimension_start, dimension_end - dimension_start));
        std::string value = statement.substr(dimension_end + 3);
        const size_t terminator = value.find_last_of(';');
        if (terminator != std::string::npos)
            value = value.substr(0, terminator);
        items = split_quoted_list(value);
        return true;
    }

    bool is_empty_data_value(std::string_view value)
    {
        return value == "." || value == "\".\"";
    }

    void append_csv_escaped(std::string &row, std::string_view field)
    {
        if (field.find_first_of(",\n\"") == std::string_view::npos)
        {
            row.append(field);
            return;
        }

        row.push_back('"');
        for (char ch : field)
        {
            if (ch == '"')
                row.push_back('"');
            row.push_back(ch);
        }
        row.push_back('"');
    }

    bool advance_indices(std::vector<size_t> &indices, const std::vector<const std::vector<std::string> *> &value_refs)
    {
        for (int d = (int)indices.size() - 1; d >= 0; --d)
        {
            if (++indices[d] < value_refs[d]->size())
                return false;
            indices[d] = 0;
        }
        return true;
    }
}

void px2csv::convert_stream_binary(std::istream &in, std::ostream &out, bool include_codes, bool include_labels, bool skip_empty)
{
    // Read the entire input into a buffer and normalize line endings in-place
    std::string raw_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    {
        size_t out = 0;
        for (size_t i = 0; i < raw_data.size(); ++i)
        {
            if (raw_data[i] == '\r')
            {
                raw_data[out++] = '\n';
                if (i + 1 < raw_data.size() && raw_data[i + 1] == '\n')
                    ++i;
            }
            else
                raw_data[out++] = raw_data[i];
        }
        raw_data.resize(out);
    }
    std::istringstream utf8_stream(std::move(raw_data));

    // --- 1. Parse metadata until DATA= ---
    std::string line, statement;
    std::vector<std::string> dimensions;
    std::unordered_map<std::string, std::vector<std::string>> dimension_values;
    std::unordered_map<std::string, std::vector<std::string>> dimension_codes;
    bool found_data = false;
    bool meta_in_quotes = false; // quote state tracked across line breaks
    while (std::getline(utf8_stream, line))
    {
        if (!meta_in_quotes && line.find("DATA=") == 0)
        {
            found_data = true;
            break;
        }
        // Add a space between accumulated lines to prevent word fusion
        if (!statement.empty())
            statement += ' ';
        statement += line;
        // Update cross-line quote state
        for (char ch : line)
            if (ch == '"')
                meta_in_quotes = !meta_in_quotes;
        // Never terminate while inside a multi-line quoted value
        if (meta_in_quotes)
            continue;
        if (!has_statement_terminator(line))
            continue;
        auto pos = statement.find('=');
        if (pos == std::string::npos)
        {
            statement.clear();
            continue;
        }
        std::string key = normalize(statement.substr(0, pos));
        std::string value = statement.substr(pos + 1);
        if (key == "STUB" || key == "HEADING")
        {
            auto dims = split_quoted_list(value);
            dimensions.insert(dimensions.end(), dims.begin(), dims.end());
        }
        std::string dim;
        std::vector<std::string> items;
        if (parse_dimension_list_statement(statement, "VALUES", dim, items))
        {
            if (dimension_values.count(dim) == 0)
                dimension_values[dim] = std::move(items);
        }
        else if (parse_dimension_list_statement(statement, "CODES", dim, items))
        {
            if (dimension_codes.count(dim) == 0)
                dimension_codes[dim] = std::move(items);
        }
        statement.clear();
    }
    if (!found_data)
        throw std::runtime_error("DATA= section not found in PX file");

    // Validate that VALUES and CODES counts match per dimension
    for (const auto &dim : dimensions)
    {
        std::string nd = normalize(dim);
        auto vit = dimension_values.find(nd);
        auto cit = dimension_codes.find(nd);
        if (vit != dimension_values.end() && cit != dimension_codes.end())
        {
            if (vit->second.size() != cit->second.size())
                throw std::runtime_error(
                    "VALUES and CODES count mismatch for dimension '" + nd + "': " + std::to_string(vit->second.size()) + " values vs " + std::to_string(cit->second.size()) + " codes");
        }
    }

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
            // Dimension has no VALUES: use the dimension name itself as the sole label
            dimension_values.emplace(nd, std::vector<std::string>{nd});
            value_refs.push_back(&dimension_values.at(nd));
        }
        if (cit != dimension_codes.end())
            code_refs.push_back(&cit->second);
        else
            code_refs.push_back(value_refs.back());
    }
    size_t n_dims = value_refs.size();
    std::vector<size_t> indices(n_dims, 0);
    std::vector<std::vector<std::string>> escaped_value_refs, escaped_code_refs;
    escaped_value_refs.reserve(n_dims);
    escaped_code_refs.reserve(n_dims);
    for (size_t i = 0; i < n_dims; ++i)
    {
        std::vector<std::string> escaped_values;
        escaped_values.reserve(value_refs[i]->size());
        for (const auto &item : *value_refs[i])
            escaped_values.push_back(csv_escape(item));
        escaped_value_refs.push_back(std::move(escaped_values));

        std::vector<std::string> escaped_codes;
        escaped_codes.reserve(code_refs[i]->size());
        for (const auto &item : *code_refs[i])
            escaped_codes.push_back(csv_escape(item));
        escaped_code_refs.push_back(std::move(escaped_codes));
    }

    // --- 4. Stream data rows ---
    bool done = false;
    std::string row;
    row.reserve(1024);
    while (std::getline(utf8_stream, line) && !done)
    {
        if (auto pos = line.find(';'); pos != std::string::npos)
            line = line.substr(0, pos);
        size_t token_start = 0;
        while (token_start < line.size())
        {
            token_start = line.find_first_not_of(" \t\n\r", token_start);
            if (token_start == std::string::npos)
                break;
            size_t token_end = line.find_first_of(" \t\n\r", token_start);
            if (token_end == std::string::npos)
                token_end = line.size();

            std::string_view value(line.data() + token_start, token_end - token_start);
            bool empty_value = is_empty_data_value(value);
            if (skip_empty && empty_value)
            {
                if (advance_indices(indices, value_refs))
                    done = true;
                token_start = token_end;
                continue;
            }
            // Write CSV row
            row.clear();
            bool first_col = true;
            for (size_t i = 0; i < n_dims; ++i)
            {
                if (include_codes && include_labels)
                {
                    if (!first_col)
                        row.push_back(',');
                    row.append(escaped_code_refs[i][indices[i]]);
                    row.push_back(',');
                    row.append(escaped_value_refs[i][indices[i]]);
                    first_col = false;
                }
                else if (include_codes || include_labels)
                {
                    if (!first_col)
                        row.push_back(',');
                    row.append(include_codes ? escaped_code_refs[i][indices[i]] : escaped_value_refs[i][indices[i]]);
                    first_col = false;
                }
            }
            row.push_back(',');
            if (!empty_value)
                append_csv_escaped(row, value);
            row.push_back('\n');
            out.write(row.data(), (std::streamsize)row.size());
            // Increment indices (cartesian product, streaming)
            if (advance_indices(indices, value_refs))
                done = true;
            token_start = token_end;
        }
    }
}

void px2csv::convert(const std::string &input_path, const std::string &output_path, bool include_codes, bool include_labels, bool skip_empty)
{
    std::ifstream in(input_path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Failed to open input file");
    std::ofstream out(output_path);
    if (!out)
        throw std::runtime_error("Failed to open output file");
    convert_stream_binary(in, out, include_codes, include_labels, skip_empty);
}
