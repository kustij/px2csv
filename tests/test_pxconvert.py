import os
import csv
import io
from px2csv import convert


def test_px2csv_12b4_file_to_file():
    """Test AXIS-VERSION 2013 PX conversion using file paths."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    output_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "../tests/test_file.csv")
    )
    with open(px_path, encoding="utf-8") as f:
        assert 'AXIS-VERSION="2013"' in f.read()
    convert(px_path, output_path, include_codes=True, include_labels=True)
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0


def test_px2csv_132g_parses_toimiala_values():
    """Test 132g.px parses Toimiala labels after a quoted semicolon."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/132g.px")
    with open(px_path, encoding="utf-8") as f:
        text = f.read()
    assert 'AXIS-VERSION="2024"' in text
    metadata = text.split("DATA=", 1)[0]

    px_data = metadata + "DATA=\n1;"

    output = io.BytesIO()
    convert(
        io.BytesIO(px_data.encode("utf-8")),
        output,
        include_codes=True,
        include_labels=True,
        skip_empty=True,
    )

    rows = list(csv.DictReader(io.StringIO(output.getvalue().decode("utf-8"))))
    assert [row["Toimiala_code"] for row in rows] == ["SSS"]
    assert rows[0]["Toimiala_label"] == "Yhteensä"


def test_px2csv_12b4_stream_to_stream():
    """Test AXIS-VERSION 2013 PX conversion using file-like objects."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    output_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "../tests/test_file_stream.csv")
    )
    with open(px_path, "rb") as f:
        fin = io.BytesIO(f.read())
    fout = io.BytesIO()
    convert(fin, fout, include_codes=True, include_labels=True)
    with open(output_path, "wb") as f:
        f.write(fout.getvalue())
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0
