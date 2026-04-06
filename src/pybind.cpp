

#include <pybind11/pybind11.h>
#include <pybind11/iostream.h>
#include <sstream>
#include "px2csv.h"

namespace py = pybind11;

PYBIND11_MODULE(px2csv, m)
{
    m.def("convert", [](py::object input, py::object output, bool include_codes = false, bool include_labels = true)
          {
        // If input/output are str, treat as file paths
        if (py::isinstance<py::str>(input) && py::isinstance<py::str>(output)) {
            std::string in_path = input.cast<std::string>();
            std::string out_path = output.cast<std::string>();
            px2csv::convert(in_path, out_path, include_codes, include_labels);
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
        px2csv::convert_stream_binary(in_stream, out_stream, include_codes, include_labels); }, py::arg("input"), py::arg("output"), py::arg("include_codes") = false, py::arg("include_labels") = true,
          R"pbdoc(
        Convert a PX (PC-Axis) file to CSV.
        Args:
            input: Input file path (str) or file-like object (must have read()).
            output: Output file path (str) or file-like object (must have write()).
            include_codes: Include code columns (default: False)
            include_labels: Include label columns (default: True)
            # Input must be UTF-8. Only UTF-8 is supported.
        )pbdoc");
}
