// Streaming: binary stream to text stream (decodes to UTF-8)
#include <stdexcept>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

    // Advance the cartesian-product odometer while incrementally maintaining the
    // count of dimensions whose current index is filtered out (keep_masks[d][idx] == 0).
    // This keeps the per-cell "is this row kept?" test at O(1) amortized.
    bool advance_indices_filtered(std::vector<size_t> &indices,
                                  const std::vector<const std::vector<std::string> *> &value_refs,
                                  const std::vector<std::vector<char>> &keep_masks,
                                  size_t &unkept_dims)
    {
        for (int d = (int)indices.size() - 1; d >= 0; --d)
        {
            const size_t old_idx = indices[d];
            const size_t new_idx = (old_idx + 1 < value_refs[d]->size()) ? old_idx + 1 : 0;
            if (!keep_masks[d][old_idx])
                --unkept_dims;
            if (!keep_masks[d][new_idx])
                ++unkept_dims;
            indices[d] = new_idx;
            if (new_idx != 0)
                return false; // no carry into the next dimension
        }
        return true; // full wrap-around: data exhausted
    }

    // True for any character that separates DATA tokens.
    inline bool is_token_delim(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // Read one logical line from the stream, consuming the terminator. Treats
    // "\n", "\r\n" and a lone "\r" (classic Mac) all as line endings, so the
    // metadata parser sees normalized lines without buffering the whole file.
    // Returns false only at end-of-input with nothing read.
    bool read_logical_line(std::istream &in, std::string &line)
    {
        line.clear();
        int c = in.get();
        if (c == std::char_traits<char>::eof())
            return false;
        while (c != std::char_traits<char>::eof())
        {
            if (c == '\n')
                break;
            if (c == '\r')
            {
                if (in.peek() == '\n')
                    in.get();
                break;
            }
            line.push_back(static_cast<char>(c));
            c = in.get();
        }
        return true;
    }
}

void px2csv::convert_stream_binary(std::istream &in, std::ostream &out, bool include_codes, bool include_labels, bool skip_empty, const SelectMap &select)
{
    // --- 1. Parse metadata until DATA= ---
    // Metadata is small; read it a logical line at a time straight from the input
    // stream (no whole-file buffering), so peak memory stays independent of the
    // potentially huge DATA section that follows.
    std::string line, statement;
    std::vector<std::string> dimensions;
    std::unordered_map<std::string, std::vector<std::string>> dimension_values;
    std::unordered_map<std::string, std::vector<std::string>> dimension_codes;
    bool found_data = false;
    bool meta_in_quotes = false; // quote state tracked across line breaks
    while (read_logical_line(in, line))
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

    // --- 3b. Build per-dimension keep masks from the select filter ---
    // A row is emitted only when every dimension's current index is kept.
    // Dimensions absent from `select` keep all of their indices.
    const bool has_filter = !select.empty();
    std::unordered_map<std::string, std::unordered_set<std::string>> select_sets;
    if (has_filter)
    {
        for (const auto &kv : select)
            select_sets.emplace(normalize(kv.first),
                                std::unordered_set<std::string>(kv.second.begin(), kv.second.end()));
    }
    std::vector<std::vector<char>> keep_masks(n_dims);
    size_t unkept_dims = 0;
    if (has_filter)
    {
        std::vector<std::string> unknown_dims;
        std::vector<std::string> unmatched_values;
        for (size_t d = 0; d < n_dims; ++d)
        {
            const size_t dim_size = value_refs[d]->size();
            const std::string nd = normalize(dimensions[d]);
            auto sit = select_sets.find(nd);
            if (sit == select_sets.end())
            {
                // No filter on this dimension: keep every index.
                keep_masks[d].assign(dim_size, 1);
                continue;
            }
            // Filter this dimension: keep only indices whose code or label matches.
            keep_masks[d].assign(dim_size, 0);
            const auto &wanted = sit->second;
            std::unordered_set<std::string> matched;
            for (size_t i = 0; i < dim_size; ++i)
            {
                const std::string &code = (*code_refs[d])[i];
                const std::string &label = (*value_refs[d])[i];
                if (wanted.count(code))
                {
                    keep_masks[d][i] = 1;
                    matched.insert(code);
                }
                else if (wanted.count(label))
                {
                    keep_masks[d][i] = 1;
                    matched.insert(label);
                }
            }
            for (const auto &w : wanted)
                if (matched.count(w) == 0)
                    unmatched_values.push_back(nd + "=" + w);
        }
        // Report select keys that do not correspond to any dimension in the file.
        for (const auto &kv : select_sets)
        {
            bool found = false;
            for (size_t d = 0; d < n_dims; ++d)
                if (normalize(dimensions[d]) == kv.first)
                {
                    found = true;
                    break;
                }
            if (!found)
                unknown_dims.push_back(kv.first);
        }
        if (!unknown_dims.empty() || !unmatched_values.empty())
        {
            std::string msg = "select filter references unknown ";
            if (!unknown_dims.empty())
            {
                msg += "dimension(s): ";
                for (size_t i = 0; i < unknown_dims.size(); ++i)
                    msg += (i ? ", " : "") + unknown_dims[i];
            }
            if (!unmatched_values.empty())
            {
                if (!unknown_dims.empty())
                    msg += "; ";
                msg += "value(s): ";
                for (size_t i = 0; i < unmatched_values.size(); ++i)
                    msg += (i ? ", " : "") + unmatched_values[i];
            }
            throw std::runtime_error(msg);
        }
        // Initialize the unkept-dimension count for the starting (all-zero) index.
        for (size_t d = 0; d < n_dims; ++d)
            if (!keep_masks[d][0])
                ++unkept_dims;
    }

    // --- 4. Stream data rows ---
    // The DATA section may be many gigabytes, so it is never buffered whole.
    // We read fixed-size chunks from the input and tokenize across chunk
    // boundaries: in-chunk tokens are handled as zero-copy string_views, and the
    // rare token that straddles a boundary is assembled in `carry`.
    std::string row;
    row.reserve(1024);

    // Emit one data value (or skip it when filtered/empty) and advance the
    // odometer. Returns true once the cartesian product is exhausted.
    auto emit_value = [&](std::string_view value) -> bool
    {
        bool empty_value = is_empty_data_value(value);
        const bool row_filtered_out = has_filter && unkept_dims != 0;
        if (row_filtered_out || (skip_empty && empty_value))
        {
            // Row is dropped: advance the odometer without emitting.
            if (has_filter)
                return advance_indices_filtered(indices, value_refs, keep_masks, unkept_dims);
            return advance_indices(indices, value_refs);
        }
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
        if (has_filter)
            return advance_indices_filtered(indices, value_refs, keep_masks, unkept_dims);
        return advance_indices(indices, value_refs);
    };

    constexpr size_t READ_CHUNK = 1u << 16; // 64 KiB
    std::vector<char> buf(READ_CHUNK);
    std::string carry; // partial token spanning a chunk boundary
    bool done = false; // all cells consumed
    bool stop = false; // ';' terminates the DATA section
    while (!done && !stop)
    {
        in.read(buf.data(), (std::streamsize)buf.size());
        const std::streamsize got = in.gcount();
        if (got <= 0)
            break;
        const char *data = buf.data();
        std::streamsize i = 0;
        while (i < got)
        {
            if (!carry.empty())
            {
                // Continue a token that began in the previous chunk.
                std::streamsize j = i;
                while (j < got && !is_token_delim(data[j]) && data[j] != ';')
                    ++j;
                carry.append(data + i, (size_t)(j - i));
                i = j;
                if (j == got)
                    break; // token still unfinished; read more
                done = emit_value(carry);
                carry.clear();
                if (data[j] == ';')
                {
                    stop = true;
                    break;
                }
                ++i; // skip the delimiter
                if (done)
                    break;
                continue;
            }
            if (data[i] == ';')
            {
                stop = true;
                break;
            }
            if (is_token_delim(data[i]))
            {
                ++i;
                continue;
            }
            // Token starts at i; scan to its end within this chunk.
            std::streamsize j = i + 1;
            while (j < got && !is_token_delim(data[j]) && data[j] != ';')
                ++j;
            if (j == got)
            {
                carry.assign(data + i, (size_t)(j - i)); // runs into next chunk
                break;
            }
            done = emit_value(std::string_view(data + i, (size_t)(j - i)));
            if (data[j] == ';')
            {
                stop = true;
                break;
            }
            i = j + 1;
            if (done)
                break;
        }
    }
    // A final token with no trailing delimiter (input ended mid-number).
    if (!done && !stop && !carry.empty())
        emit_value(carry);
}

void px2csv::convert(const std::string &input_path, const std::string &output_path, bool include_codes, bool include_labels, bool skip_empty, const SelectMap &select)
{
    std::ifstream in(input_path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Failed to open input file");
    std::ofstream out(output_path);
    if (!out)
        throw std::runtime_error("Failed to open output file");
    convert_stream_binary(in, out, include_codes, include_labels, skip_empty, select);
}
