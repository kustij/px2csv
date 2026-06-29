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

### Filter rows while converting (`select`)

Pass `select` to keep only the rows you need. Filtering happens inside the C++
converter, so the discarded rows are never materialized.

```python
from px2csv import convert
convert(
	"examples/12b4.px",
	"examples/12b4.csv",
	include_codes=True,
	include_labels=True,
	select={
		"Tiedot": ["arvogwh", "osuuskk"],
		"Vuosi": ["2024", "2023"],
	},
)
```

`select` maps a dimension name to the list of value **codes** to keep; Only rows that match every selected dimension are
emitted; dimensions omitted from `select` are kept in full.
