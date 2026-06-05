cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fsk_demod)

# Always compile the Mock Environment and the DSP file
target_sources(app PRIVATE mock_adc.c dsp.c)

# Conditionally compile the entry point based on the unit test flag
if(BUILD_UNIT_TEST)
    message(STATUS "=== BUILDING FILTER UNIT TEST ===")
    target_sources(app PRIVATE test_filter.c)
else()
    message(STATUS "=== BUILDING MAIN APPLICATION ===")
    target_sources(app PRIVATE main.c)
endif()