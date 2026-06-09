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

    if os.path.exists("/environment/headers"):
        include_dir = "/environment/headers"
    else:
        include_dir = "/app/headers"

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


# Parametrize: Target Amp, Int Amp, Noise StdDev, Int Freq Offset, Description
@pytest.mark.parametrize(
    "target_amp, interferer_amp, noise_stddev, int_freq, description",
    [
        (500.0, 0.0, 0.0, 15.0, "Clean Signal"),
        (
            500.0,
            1000.0,
            0.0,
            15.0,
            "Interference Only (+15Hz)",
        ),  # Adjusted to physical limits of the FIR
        (500.0, 1000.0, 8944.0, 15.0, "Full Real-World (+15Hz)"),
        (0.0, 1581.0, 0.0, 10.0, "Squelch Test - No Signal, +10Hz Interferer"),
        (0.0, 0.0, 8944.0, 15.0, "Squelch Test - No Signal, Heavy Noise Only"),
    ],
)
def test_milestone_1_dsp_mixer(
    compile_runner, target_amp, interferer_amp, noise_stddev, int_freq, description
):
    output_bin = compile_runner

    print(f"\n--- Running Milestone 1 Test: {description} ---")

    run_cmd = [
        output_bin,
        str(target_amp),
        str(interferer_amp),
        str(noise_stddev),
        str(int_freq),
    ]
    run_result = subprocess.run(run_cmd, capture_output=True, text=True, timeout=15)

    print(run_result.stdout)

    if run_result.returncode != 0:
        print(f"STDERR: {run_result.stderr}")

    assert run_result.returncode == 0, f"FIR Filter failed during: {description}"
    assert "PASS:" in run_result.stdout, "Runner did not output the PASS condition."
