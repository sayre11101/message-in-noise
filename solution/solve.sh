bash
#!/bin/bash
set -euo pipefail

if [ -e /oracle/main.c ]; then
    echo "Found main in /oracle/"
    DIR="oracle"
elif [ -e /solution/main.c ]; then
    echo "Found main in /solution/"
    DIR="solution"
else
    echo "ERROR: Could not find mounted solution directory under /oracle or /solution" >&2
    exit 1
fi

echo "---- contents of ${DIR} --------"
ls -la "/${DIR}/"
echo "--------------------------------"

# The Oracle simply copies the known-good code into the Agent's output directory
cp /${DIR}/main.c /app/
cp /${DIR}/dsp.c /app/
cp /${DIR}/dsp.h /app/
cp /${DIR}/mock_adc.c /app/
cp /${DIR}/CMakeLists.txt /app/

echo "---- /app/main.c after overwriting dummy main.c -----"
cat /app/main.c
echo "-----------------------------------------------------"

