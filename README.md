# PxConvert

Fast and memory-efficient Python/C++ converter for PX (PC-Axis) files.

Currently we are supporting only CSV files in UTF-8 format.

### Convert using file paths

```python
from pxconvert import convert
convert("examples/sample.px", "examples/sample.csv", include_codes=True, include_labels=True)
```

### Convert using file-like objects (streams)

```python
import io
from pxconvert import convert
with open("examples/sample.px", "rb") as fin, open("examples/sample.csv", "wb") as fout:
	convert(fin, fout, include_codes=True, include_labels=True)
```
