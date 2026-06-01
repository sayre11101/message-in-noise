cmake_minimum_required(VERSION 3.20.0)

# Strictly check which phase is currently compiling the code
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL "/tmp/eval")
    # Phase 2: Python Testing (Flattened files)
    set(MOCK_SRC "mock_adc.c")
else()
    # Phase 1: Harbor Oracle Validation (Mounted files)
    set(MOCK_SRC "/tests/mock_adc.c")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fsk_message)

target_sources(app PRIVATE main.c)
target_sources(app PRIVATE ${MOCK_SRC})