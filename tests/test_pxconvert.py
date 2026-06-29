import os
import csv
import io
import pytest
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


def _convert_to_rows(px_path, **kwargs):
    with open(px_path, "rb") as f:
        fin = io.BytesIO(f.read())
    fout = io.BytesIO()
    convert(fin, fout, **kwargs)
    return list(csv.DictReader(io.StringIO(fout.getvalue().decode("utf-8"))))


def test_select_filters_by_code():
    """select keeps only rows whose dimension code matches."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    rows = _convert_to_rows(
        px_path,
        include_codes=True,
        include_labels=True,
        select={"Vuosi": ["2000"], "Tiedot": ["arvogwh"]},
    )
    assert rows, "expected at least one filtered row"
    assert {row["Vuosi_code"] for row in rows} == {"2000"}
    assert {row["Tiedot_code"] for row in rows} == {"arvogwh"}


def test_select_matches_full_output_subset():
    """Filtered output equals the unfiltered output restricted to the kept codes."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    full = _convert_to_rows(px_path, include_codes=True, include_labels=True)
    filtered = _convert_to_rows(
        px_path,
        include_codes=True,
        include_labels=True,
        select={"Tiedot": ["arvogwh"]},
    )
    expected = [r for r in full if r["Tiedot_code"] == "arvogwh"]
    assert filtered == expected


def test_select_by_label():
    """select can match value labels in addition to codes."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    rows = _convert_to_rows(
        px_path,
        include_codes=True,
        include_labels=True,
        select={"Vuosi": ["2000"], "Tiedot": ["Määrä, GWh"]},
    )
    assert rows
    assert {row["Tiedot_label"] for row in rows} == {"Määrä, GWh"}


def test_select_unknown_dimension_raises():
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    with pytest.raises(Exception):
        _convert_to_rows(px_path, select={"DoesNotExist": ["x"]})


def test_select_unknown_value_raises():
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    with pytest.raises(Exception):
        _convert_to_rows(px_path, select={"Vuosi": ["1066"]})


def test_select_none_is_no_filter():
    px_path = os.path.join(os.path.dirname(__file__), "../examples/12b4.px")
    a = _convert_to_rows(px_path, include_codes=True, select=None)
    b = _convert_to_rows(px_path, include_codes=True)
    assert a == b


def _build_large_px(n_a, n_b):
    """Build a synthetic PX whose DATA section is one unbroken line far larger
    than the converter's internal read chunk, with 5-digit values so tokens
    straddle the 64 KiB boundary. Returns (px_bytes, total_cells)."""
    a_codes = [str(i) for i in range(n_a)]
    b_codes = [str(i) for i in range(n_b)]
    a_vals = [f"a{i}" for i in range(n_a)]
    b_vals = [f"b{i}" for i in range(n_b)]
    total = n_a * n_b
    # Row-major over [A, B]: cell k -> A=k//n_b, B=k%n_b. Value = 10000 + k.
    data = " ".join(str(10000 + k) for k in range(total)) + ";"
    meta = (
        'STUB="A","B";\n'
        + 'VALUES("A")='
        + ",".join(f'"{v}"' for v in a_vals)
        + ";\n"
        + 'VALUES("B")='
        + ",".join(f'"{v}"' for v in b_vals)
        + ";\n"
        + 'CODES("A")='
        + ",".join(f'"{c}"' for c in a_codes)
        + ";\n"
        + 'CODES("B")='
        + ",".join(f'"{c}"' for c in b_codes)
        + ";\n"
        + "DATA=\n"
    )
    return (meta + data).encode("utf-8"), total


def _convert_bytes_to_rows(px_bytes, **kwargs):
    fout = io.BytesIO()
    convert(io.BytesIO(px_bytes), fout, **kwargs)
    return list(csv.DictReader(io.StringIO(fout.getvalue().decode("utf-8"))))


def test_streaming_across_read_chunks():
    """DATA spanning many 64 KiB chunks (single line) is tokenized correctly."""
    px_bytes, total = _build_large_px(200, 200)
    assert len(px_bytes) > (1 << 16) * 3  # several read chunks
    rows = _convert_bytes_to_rows(px_bytes, include_codes=True, include_labels=True)
    assert len(rows) == total
    # First and last cells round-trip, proving boundary tokens aren't dropped/split.
    assert rows[0]["A_code"] == "0" and rows[0]["B_code"] == "0"
    assert rows[0]["value"] == "10000"
    assert rows[-1]["A_code"] == "199" and rows[-1]["B_code"] == "199"
    assert rows[-1]["value"] == str(10000 + total - 1)
    # Spot-check a value mid-stream against the row-major formula.
    mid = total // 2 + 7
    assert rows[mid]["value"] == str(10000 + mid)
    assert rows[mid]["A_code"] == str(mid // 200)
    assert rows[mid]["B_code"] == str(mid % 200)


def test_streaming_across_read_chunks_with_select():
    """Filtering still works when DATA crosses many read-chunk boundaries."""
    px_bytes, total = _build_large_px(200, 200)
    rows = _convert_bytes_to_rows(px_bytes, include_codes=True, select={"A": ["100"]})
    assert len(rows) == 200
    assert {r["A_code"] for r in rows} == {"100"}
    assert rows[0]["value"] == str(10000 + 100 * 200)
