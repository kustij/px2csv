from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "pxconvert.pxconvert",  # pxconvert/pxconvert.so
        ["src/pybind.cpp", "src/pxconvert.cpp"],
        include_dirs=[
            pybind11.get_include(),
            "src",
        ],
        language="c++",
        extra_compile_args=["-O3", "-std=c++17"],
    ),
]

setup(
    name="pxconvert",
    version="0.1.0",
    description="PX file converter with Azure Blob streaming support",
    packages=["pxconvert"],
    ext_modules=ext_modules,
    zip_safe=False,
)
