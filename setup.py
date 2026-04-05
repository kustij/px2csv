from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "px2csv.px2csv",  # px2csv/px2csv.so
        ["src/pybind.cpp", "src/px2csv.cpp"],
        include_dirs=[
            pybind11.get_include(),
            "src",
        ],
        language="c++",
        extra_compile_args=["-O3", "-std=c++17"],
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
