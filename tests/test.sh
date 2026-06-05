#!/bin/bash
set -euo pipefail 
# tests/test.sh

mkdir -p /logs/verifier
mkdir -p /tmp/eval

echo "========================================"
echo "          AGENT GENERATED main.c          "
echo "========================================"
cat /app/main.c
echo -e "\n========================================"

echo "========================================"
echo "          AGENT GENERATED dsp.c          "
echo "========================================"
cat /app/dsp.c
echo -e "\n========================================"

# 3. Compile the Zephyr application
echo "Building Zephyr application..."
set +e 
west build -b native_sim .
if [ $? -ne 0 ]; then
    echo "FAIL: Application failed to compile."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi

# ========================================================
# 4. RUN ONCE AND SAVE TO FILE
echo "=== RUNNING SIMULATION ==="
# 'timeout' prevents infinite loops.
# 'tee' prints to the terminal AND saves to the file Pytest is looking for.
timeout -k 5 45 ./build/zephyr/zephyr.exe | tee /tmp/eval/agent_output.txt

# Capture the exit code of the simulation
EXIT_CODE=${PIPESTATUS[0]}

echo "=========================="
# ========================================================

# Check if it timed out or crashed natively
if [ $EXIT_CODE -ne 0 ] && [ $EXIT_CODE -ne 124 ]; then
    echo "FAIL: Runtime Error (Code $EXIT_CODE)."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi

# 5. Run the Python grader
echo "Executing Python test suite..."
ulimit -n 1024

uvx \
  --offline \
  -p 3.12 \
  -w pytest==8.4.1 \
  -w pytest-json-ctrf==0.3.5 \
  pytest --ctrf /logs/verifier/ctrf.json /tests/test_outputs.py -rA -v

# 6. Write the final reward file based on pytest's exit code
if [ $? -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi