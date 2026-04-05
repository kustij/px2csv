#pragma once
#include <string>
#include <istream>
#include <ostream>

class pxconvert
{
public:
    // Streaming: binary stream to text stream (decodes to UTF-8)
    static void convert_stream_binary(std::istream &in, std::ostream &out, bool include_codes = false, bool include_labels = true);
    // Convenience: file to file
    static void convert(const std::string &input_path, const std::string &output_path, bool include_codes = false, bool include_labels = true);
};
