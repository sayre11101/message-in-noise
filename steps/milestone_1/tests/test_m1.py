import subprocess
import os
import pytest


@pytest.fixture(scope="module", autouse=True)
def compile_runner():
    print("\n--- Compiling Milestone 1 ---")
    if os.path.exists("/solution/dsp_mixer.c"):
        agent_code = "/solution/dsp_mixer.c"
        print("[INFO] Compiling ORACLE from /solution/")
    else:
        agent_code = "/app/dsp_mixer.c"
        print("[INFO] Compiling AGENT from /app/")

    test_runner = "/tests/test_m1_runner.c"
    output_bin = "/tmp/test_m1.out"

    include_dir = (
        "/environment/headers"
        if os.path.exists("/environment/headers")
        else "/app/headers"
    )

    compile_cmd = [
        "gcc",
        "-Wall",
        "-O3",
        "-I",
        include_dir,
        agent_code,
        test_runner,
        "-o",
        output_bin,
        "-lm",
    ]
    compile_result = subprocess.run(compile_cmd, capture_output=True, text=True)
    assert compile_result.returncode == 0, (
        f"Compilation failed:\n{compile_result.stderr}"
    )

    yield output_bin


@pytest.mark.parametrize(
    "mode, sig_amp, sig_offset, int_amp, int_offset, noise_stddev, description",
    [
        (0, 500.0, 0.0, 0.0, 0.0, 0.0, "Measurement: Center Passband Gain (0.0 Hz)"),
        (0, 500.0, 0.9, 0.0, 0.0, 0.0, "Measurement: FSK Tone Gain (0.9 Hz)"),
        (0, 0.0, 0.0, 500.0, 15.0, 0.0, "Measurement: Stopband Gain (15.0 Hz)"),
        (0, 0.0, 0.0, 0.0, 0.0, 8944.0, "Measurement: Pure Noise Gain"),
        (1, 500.0, 0.9, 0.0, 15.0, 0.0, "Isolation: Clean Signal"),
        (1, 500.0, 0.9, 1000.0, 15.0, 8944.0, "Isolation: Full Real-World"),
        (2, 500.0, 0.0, 0.0, 0.0, 0.0, "S-Curve & Filter Gain Sweep (-10Hz to +10Hz)"),
    ],
)
def test_milestone_1_dsp_mixer(
    compile_runner,
    mode,
    sig_amp,
    sig_offset,
    int_amp,
    int_offset,
    noise_stddev,
    description,
):
    output_bin = compile_runner
    print(f"\n--- Running Milestone 1 Test: {description} ---")

    run_cmd = [
        output_bin,
        str(mode),
        str(sig_amp),
        str(sig_offset),
        str(int_amp),
        str(int_offset),
        str(noise_stddev),
    ]

    # INCREASED TIMEOUT: The 21-step sweep simulates 105 seconds of RF data.
    # We give Python 300 seconds to let the C code finish the math.
    run_result = subprocess.run(run_cmd, capture_output=True, text=True, timeout=300)

    print(run_result.stdout)
    if run_result.returncode != 0:
        print(f"STDERR: {run_result.stderr}")

    assert run_result.returncode == 0, f"FIR Filter failed during: {description}"
    assert "PASS:" in run_result.stdout, "Runner did not output the PASS condition."
