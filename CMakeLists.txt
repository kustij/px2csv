cmake_minimum_required(VERSION 3.10)
project(px2csv)

set(CMAKE_CXX_STANDARD 17)

# Pybind11 extension
find_package(pybind11 REQUIRED)
pybind11_add_module(px2csv_py
    src/pybind.cpp
    src/px2csv.cpp
    src/px2csv.h
)

# Set output name for Python extension (so it becomes px2csv/px2csv.so)
set_target_properties(px2csv_py PROPERTIES
    OUTPUT_NAME px2csv
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/px2csv
)

# Install rules for Python packaging
install(TARGETS px2csv_py LIBRARY DESTINATION px2csv)
install(FILES px2csv/__init__.py DESTINATION px2csv)
