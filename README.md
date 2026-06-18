# px2csv

Convert PX (PC-Axis) files to CSV fast and memory-efficiently in Python.

We currently support only UTF-8 encoded PX files.

### Install from PyPI

```python
pip install px2csv
```

### Convert using file paths

```python
from px2csv import convert
convert("examples/12b4.px", "examples/12b4.csv", include_codes=True, include_labels=True)
```

For large sparse PX files, skip missing values and avoid repeated label columns:

```python
from px2csv import convert
convert(
	"examples/132g.px",
	"examples/132g.csv",
	include_codes=True,
	include_labels=False,
	skip_empty=True,
)
```

### Convert using file-like objects (streams)

```python
import io
from px2csv import convert
with open("examples/12b4.px", "rb") as fin, open("examples/12b4.csv", "wb") as fout:
	convert(fin, fout, include_codes=True, include_labels=True)
```
