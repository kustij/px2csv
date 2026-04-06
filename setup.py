from setuptools import setup, Extension
import pybind11
import sys

extra_compile_args = ["-O3"]
if sys.platform == "win32":
    extra_compile_args.append("/std:c++17")
else:
    extra_compile_args.append("-std=c++17")

ext_modules = [
    Extension(
        "px2csv.px2csv",  # px2csv/px2csv.so
        ["src/pybind.cpp", "src/px2csv.cpp"],
        include_dirs=[
            pybind11.get_include(),
            "src",
        ],
        language="c++",
        extra_compile_args=extra_compile_args,
    ),
]

setup(
    name="px2csv",
    version="0.1.0",
    description="Convert PX files to CSV",
    packages=["px2csv"],
    ext_modules=ext_modules,
    zip_safe=False,
)
