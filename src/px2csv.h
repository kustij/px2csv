#pragma once
#include <string>
#include <istream>
#include <ostream>
#include <unordered_map>
#include <vector>

class px2csv
{
public:
    // Map of dimension name -> list of allowed value codes/labels to keep.
    // An empty map means no filtering (emit every row).
    using SelectMap = std::unordered_map<std::string, std::vector<std::string>>;

    // Streaming: binary stream to text stream (decodes to UTF-8)
    static void convert_stream_binary(std::istream &in, std::ostream &out, bool include_codes = false, bool include_labels = true, bool skip_empty = false, const SelectMap &select = {});
    // Convenience: file to file
    static void convert(const std::string &input_path, const std::string &output_path, bool include_codes = false, bool include_labels = true, bool skip_empty = false, const SelectMap &select = {});
};
