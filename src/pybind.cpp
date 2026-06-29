

#include <pybind11/pybind11.h>
#include <pybind11/iostream.h>
#include <sstream>
#include <string>
#include <vector>
#include "px2csv.h"

namespace py = pybind11;

namespace
{
    // Convert an optional Python mapping {dimension: iterable[str]} into a SelectMap.
    px2csv::SelectMap parse_select(const py::object &select)
    {
        px2csv::SelectMap result;
        if (select.is_none())
            return result;
        if (!py::isinstance<py::dict>(select))
            throw std::runtime_error("select must be a dict mapping dimension name to a list of value codes/labels");
        for (auto item : select.cast<py::dict>())
        {
            std::string dim = py::str(item.first);
            std::vector<std::string> values;
            for (auto v : py::reinterpret_borrow<py::object>(item.second))
                values.emplace_back(py::str(v));
            result.emplace(std::move(dim), std::move(values));
        }
        return result;
    }
}

PYBIND11_MODULE(px2csv, m)
{
    m.def("convert", [](py::object input, py::object output, bool include_codes = false, bool include_labels = true, bool skip_empty = false, py::object select = py::none())
          {
        px2csv::SelectMap select_map = parse_select(select);
        // If input/output are str, treat as file paths
        if (py::isinstance<py::str>(input) && py::isinstance<py::str>(output)) {
            std::string in_path = input.cast<std::string>();
            std::string out_path = output.cast<std::string>();
            // Pure C++ work: release the GIL so other Python threads (e.g. the web
            // server) keep running and the app stays responsive during conversion.
            py::gil_scoped_release release;
            px2csv::convert(in_path, out_path, include_codes, include_labels, skip_empty, select_map);
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
            // Flush to Python in chunks instead of once per row: each flush is a
            // Python call under the GIL, so batching amortizes that cost to ~zero.
            // 64 KiB captures nearly all the gain while staying friendly to
            // incremental/streaming consumers (matches a typical OS pipe size).
            enum : size_t { FLUSH_THRESHOLD = 1u << 16 }; // 64 KiB
        protected:
            // Called for bulk writes (e.g. out.write(data, n)) — avoids char-by-char overhead
            std::streamsize xsputn(const char *s, std::streamsize n) override {
                buffer.append(s, n);
                if (buffer.size() >= FLUSH_THRESHOLD)
                    sync();
                return n;
            }
            int overflow(int c) override {
                if (c != EOF) {
                    buffer += static_cast<char>(c);
                    if (buffer.size() >= FLUSH_THRESHOLD)
                        sync();
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
                px2csv::convert_stream_binary(in_stream, out_stream, include_codes, include_labels, skip_empty, select_map); }, py::arg("input"), py::arg("output"), py::arg("include_codes") = false, py::arg("include_labels") = true, py::arg("skip_empty") = false, py::arg("select") = py::none(),
          R"pbdoc(
        Convert a PX (PC-Axis) file to CSV.
        Args:
            input: Input file path (str) or file-like object (must have read()).
            output: Output file path (str) or file-like object (must have write()).
            include_codes: Include code columns (default: False)
            include_labels: Include label columns (default: True)
                        skip_empty: Skip rows whose value is missing (.) (default: False)
            select: Optional dict mapping a dimension name to the list of value
                    codes (or labels) to keep, e.g. {"Tiedot": ["ntp-cp"]}.
                    Only rows matching every selected dimension are emitted, so
                    filtering happens in C++ before any CSV is produced. Unknown
                    dimensions or values raise an error. (default: None = no filter)
            # Input must be UTF-8. Only UTF-8 is supported.
        )pbdoc");
}
