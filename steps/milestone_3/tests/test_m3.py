import subprocess
import os
import shutil


def test_milestone_3_zephyr_integration():
    """
    Evaluates the complete Zephyr RTOS system integration.
    Compiles main.c, dsp_mixer.c, fsk_slicer.c, and mock_adc.c using west.
    """
    print("\n--- Staging Zephyr Environment ---")

    # In Zephyr, we usually copy the mock files into the agent's /app directory
    # right before compilation so CMake picks everything up natively.
    mock_adc_src = "/tests/mock_adc.c"
    mock_adc_dest = "/app/mock_adc.c"

    if os.path.exists(mock_adc_src):
        shutil.copy(mock_adc_src, mock_adc_dest)

    print("\n--- Building Zephyr native_sim ---")

    build_cmd = ["west", "build", "-p", "always", "-b", "native_sim", "/app"]

    # West build can take a moment, give it a generous timeout
    build_result = subprocess.run(
        build_cmd, capture_output=True, text=True, timeout=120
    )

    if build_result.returncode != 0:
        print(f"BUILD STDOUT:\n{build_result.stdout}")
        print(f"BUILD STDERR:\n{build_result.stderr}")
        assert False, "Zephyr West build failed to compile the project."

    print("\n--- Executing Zephyr native_sim ---")

    exe_path = "/app/build/zephyr/zephyr.exe"
    assert os.path.exists(exe_path), "Zephyr executable was not generated!"

    # Run the compiled native simulation
    run_result = subprocess.run([exe_path], capture_output=True, text=True, timeout=30)

    stdout = run_result.stdout
    print(stdout)

    # The ultimate assertion: did they print the correctly decoded string?
    # Your mock_adc.c prints [MOCK_ENV] SECRET_WORD: <word>
    # We parse the secret word from the mock output, then check if DECODED matches.

    secret_word = None
    for line in stdout.splitlines():
        if "[MOCK_ENV] SECRET_WORD:" in line:
            secret_word = line.split("SECRET_WORD:")[1].strip()
            break

    assert secret_word is not None, "Mock environment failed to generate a SECRET_WORD."

    expected_decoded_string = f"DECODED: {secret_word}"

    assert expected_decoded_string in stdout, (
        f"Integration failed! Expected to find '{expected_decoded_string}' in output."
    )
