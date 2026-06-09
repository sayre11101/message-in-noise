cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hardware_pll)

# 1. Add the agent's source files
target_sources(app PRIVATE 
    main.c 
    dsp_mixer.c
    fsk_slicer.c
)

# 2. Add the hardware emulator injected by the test suite
target_sources(app PRIVATE 
    mock_generator.c
)

# 3. Defensive Include Paths (Matches our Python grader logic)
target_include_directories(app PRIVATE 
    /environment/headers
    /app/headers
    /app
    ${CMAKE_CURRENT_SOURCE_DIR}
)