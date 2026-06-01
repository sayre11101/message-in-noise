import os
import re
import pytest

# Fallback paths to guarantee we find the log file
POSSIBLE_PATHS = [
    "/tmp/eval/agent_output.txt",
    "/tmp/eval/simulation_logs.txt",
    "agent_output.txt",
    "simulation_logs.txt",
    "/logs/verifier/agent_output.txt",
]


@pytest.fixture(scope="module")
def trace_data():
    """Parses the log file for the target mock parameters and the agent's output."""
    log_path = None
    for p in POSSIBLE_PATHS:
        if os.path.exists(p):
            log_path = p
            break

    assert log_path is not None, (
        "FAIL: Agent output log not found in any expected directory."
    )

    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        log_content = f.read()

    # Dynamically grab the expected word from the C mock's boot print
    secret_word_match = re.search(r"\[MOCK_ENV\] SECRET_WORD:\s*(.+)", log_content)
    assert secret_word_match is not None, (
        "FAIL: Could not find [MOCK_ENV] SECRET_WORD tag in logs."
    )

    expected_word = secret_word_match.group(1).strip()

    return expected_word, log_content


def test_compilation_success(trace_data):
    """Verify the agent's code compiled and produced output."""
    _, log_content = trace_data
    assert len(log_content) > 0, (
        "FAIL: No simulation output produced. Did the code compile?"
    )


def test_fsk_demodulation(trace_data):
    """Strictly verify the full message was correctly decoded."""
    expected_word, log_content = trace_data

    expected_output_string = f"DECODED: {expected_word}"
    pattern = rf"^DECODED: {re.escape(expected_word)}$"

    # Check if the agent's decoded string exactly matches the expected string
    assert re.search(pattern, log_content, re.MULTILINE), (
        f"FAIL: The agent failed to decode the FSK message.\n"
        f"Expected to find: '{expected_output_string}'\n"
        f"Check if the DSP algorithm locked correctly and synchronized to the baud rate."
    )


def test_output_contains_printable_ascii(trace_data):
    """Verify decoded output contains only printable ASCII characters."""
    _, log_content = trace_data
    match = re.search(r"DECODED: (.+)", log_content)
    assert match is not None, "No DECODED line found in output"
    decoded = match.group(1)
    assert all(32 <= ord(c) <= 126 for c in decoded), (
        f"Non-printable characters in output: {repr(decoded)}"
    )


def test_decoded_length_reasonable(trace_data):
    """Verify decoded output has a plausible length (not empty/truncated)."""
    expected_word, log_content = trace_data
    match = re.search(r"DECODED: (.+)", log_content)
    assert match is not None, "No DECODED line found in output"
    decoded = match.group(1)
    assert len(decoded) >= len(expected_word) // 2, (
        f"Decoded too short ({len(decoded)} chars vs expected {len(expected_word)})"
    )
