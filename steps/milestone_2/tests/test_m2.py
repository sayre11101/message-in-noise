import subprocess
import os


def test_milestone_2_fsk_slicer():
    """
    Evaluates the agent's UART 8N1 state machine and clock recovery.
    Compiles the agent's slicer against the simulated zero-crossing runner.
    """
    print("\n--- Compiling Milestone 2 ---")

    # Dynamically select the source file: Oracle first, then Agent fallback
    if os.path.exists("/solution/fsk_slicer.c"):
        agent_code = "/solution/fsk_slicer.c"
        print("[INFO] Compiling ORACLE from /solution/")
    else:
        agent_code = "/app/fsk_slicer.c"
        print("[INFO] Compiling AGENT from /app/")

    test_runner = "/tests/test_m2_runner.c"
    output_bin = "/tmp/test_m2.out"

    # Fallback logic for headers just in case
    if os.path.exists("/environment/headers"):
        include_dir = "/environment/headers"
    else:
        include_dir = "/app/headers"

    # Compile the C files
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
    ]

    compile_result = subprocess.run(compile_cmd, capture_output=True, text=True)

    assert compile_result.returncode == 0, (
        f"Compilation failed:\n{compile_result.stderr}"
    )

    print("\n--- Running Milestone 2 Test ---")
    run_result = subprocess.run([output_bin], capture_output=True, text=True, timeout=5)

    print(run_result.stdout)

    if run_result.returncode != 0:
        print(f"STDERR: {run_result.stderr}")

    assert run_result.returncode == 0, (
        "UART Slicer failed to properly frame and decode the test message."
    )
    assert "PASS:" in run_result.stdout, "Runner did not output the PASS condition."
