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
convert("examples/sample.px", "examples/sample.csv", include_codes=True, include_labels=True)
```

### Convert using file-like objects (streams)

```python
import io
from px2csv import convert
with open("examples/sample.px", "rb") as fin, open("examples/sample.csv", "wb") as fout:
	convert(fin, fout, include_codes=True, include_labels=True)
```
