import os
import pytest
from px2csv import convert


def test_px2csv_file_to_file():
    """Test PX to CSV conversion using file paths (minimal interface)."""
    px_path = os.path.join(os.path.dirname(__file__), "../examples/sample.px")
    output_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "../tests/test_file.csv")
    )
    convert(px_path, output_path, include_codes=True, include_labels=True)
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0


def test_px2csv_stream_to_stream():
    """Test PX to CSV conversion using file-like objects (minimal interface)."""
    import io

    px_path = os.path.join(os.path.dirname(__file__), "../examples/sample.px")
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
