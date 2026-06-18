from setuptools import setup, Extension
import pybind11
import sys

if sys.platform == "win32":
    # /O2: optimize for speed  /GL: whole-program opt off (avoids slow LTCG link)
    extra_compile_args = ["/O2", "/std:c++17"]
    extra_link_args = []
else:
    extra_compile_args = ["-O3", "-std=c++17"]
    extra_link_args = []

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
        extra_link_args=extra_link_args,
    ),
]

setup(
    ext_modules=ext_modules,
    zip_safe=False,
)
