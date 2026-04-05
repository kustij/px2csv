cmake_minimum_required(VERSION 3.10)
project(pxconvert)

set(CMAKE_CXX_STANDARD 17)

# Pybind11 extension
find_package(pybind11 REQUIRED)
pybind11_add_module(pxconvert_py
    src/pybind.cpp
    src/pxconvert.cpp
    src/pxconvert.h
)

# Set output name for Python extension (so it becomes pxconvert/pxconvert.so)
set_target_properties(pxconvert_py PROPERTIES
    OUTPUT_NAME pxconvert
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pxconvert
)

# Install rules for Python packaging
install(TARGETS pxconvert_py LIBRARY DESTINATION pxconvert)
install(FILES pxconvert/__init__.py DESTINATION pxconvert)
