

#include <pybind11/pybind11.h>
#include <pybind11/iostream.h>
#include <sstream>
#include "px2csv.h"

namespace py = pybind11;

PYBIND11_MODULE(px2csv, m)
{
    m.def("convert", [](py::object input, py::object output, bool include_codes = false, bool include_labels = true, bool skip_empty = false)
          {
        // If input/output are str, treat as file paths
        if (py::isinstance<py::str>(input) && py::isinstance<py::str>(output)) {
            std::string in_path = input.cast<std::string>();
            std::string out_path = output.cast<std::string>();
            px2csv::convert(in_path, out_path, include_codes, include_labels, skip_empty);
            return;
        }
        // Otherwise, treat as file-like objects (must have read()/write())
        py::scoped_ostream_redirect stream_redirect;
        // Read all input from Python file-like object
        py::object read_method = input.attr("read");
        py::bytes pydata = read_method();
        std::string data = pydata;
        std::istringstream in_stream(data);
        // Output: wrap Python file-like object
        class pyostreambuf : public std::streambuf {
            py::object pywrite;
            std::string buffer;
        protected:
            // Called for bulk writes (e.g. out.write(data, n)) — avoids char-by-char overhead
            std::streamsize xsputn(const char *s, std::streamsize n) override {
                buffer.append(s, n);
                // Flush after each complete row (ends with \n)
                if (!buffer.empty() && buffer.back() == '\n')
                    sync();
                return n;
            }
            int overflow(int c) override {
                if (c != EOF) {
                    buffer += static_cast<char>(c);
                    if (c == '\n') sync();
                }
                return c;
            }
            int sync() override {
                if (!buffer.empty()) {
                    pywrite(py::bytes(buffer));
                    buffer.clear();
                }
                return 0;
            }
        public:
            pyostreambuf(py::object pywrite_) : pywrite(pywrite_) {}
            ~pyostreambuf() { sync(); }
        };
        pyostreambuf obuf(output.attr("write"));
        std::ostream out_stream(&obuf);
                px2csv::convert_stream_binary(in_stream, out_stream, include_codes, include_labels, skip_empty); }, py::arg("input"), py::arg("output"), py::arg("include_codes") = false, py::arg("include_labels") = true, py::arg("skip_empty") = false,
          R"pbdoc(
        Convert a PX (PC-Axis) file to CSV.
        Args:
            input: Input file path (str) or file-like object (must have read()).
            output: Output file path (str) or file-like object (must have write()).
            include_codes: Include code columns (default: False)
            include_labels: Include label columns (default: True)
                        skip_empty: Skip rows whose value is missing (.) (default: False)
            # Input must be UTF-8. Only UTF-8 is supported.
        )pbdoc");
}
