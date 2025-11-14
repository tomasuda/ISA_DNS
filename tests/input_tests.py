import pytest
import socket
import random
import struct
import time
import subprocess
import os
import signal
import time
from pathlib import Path


path = str(Path(__file__).resolve().parent.parent / "dns_test")
file_correct = "test_datafile.txt"
file_incorrect_dash = "test_datafile_incorrect.txt"
file_incorrect_dot = "test_datafile_incorrect2.txt"
file_nonexistent = "nonexistent.txt"




def run_command_with_timeout(command, timeout=3):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        output, error = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            process.terminate()  # SIGTERM
        except OSError:
            pass
        time.sleep(0.5)
        output, error = process.communicate()
    return output, error, process.returncode


def test_correct_arguments():
    cmd = [path, "-f", file_correct, "-s", "8.8.8.8", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 0


def test_missing_argument():
    """Test that the program handles missing arguments."""
    cmd = [path, "-f", file_correct]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_invalid_arguments():
    """Test that the program handles invalid arguments."""
    cmd = [path, "-f", file_correct, "-k", "8.8.8.8", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_missing_file_argument():
    """Test that the program handles missing file."""
    cmd = [path, "-f", "-s", "8.8.8.8", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_missing_server_argument():
    """Test that the program handles missing server."""
    cmd = [path, "-f", file_correct, "-s", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_missing_port_number():
    """Test that the program handles missing port number."""
    cmd = [path, "-f", file_correct, "-s", "8.8.8.8", "-p"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_help_flag():
    """Test that the program displays help information."""
    cmd = [path, "-h"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 0


def test_help_with_extra_arguments():
    """Test that the program handles help flag with extra arguments."""
    cmd = [path, "-f", file_correct, "-s", "8.8.8.8", "-p", "5353", "-h"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_wrong_port_number():
    """Test that the program handles wrong port number."""
    cmd = [path, "-f", file_correct, "-s", "8.8.8.8", "-p", "70000"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_non_integer_port_number():
    """Test that the program handles non-integer port number."""
    cmd = [path, "-f", file_correct, "-s", "8.8.8.8", "-p", "jkghadfs"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_negative_port_number():
    """Test that the program handles negative port number."""
    cmd = [path, "-f", file_correct, "-s", "8.8.8.8", "-p", "-1"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_wrong_server_address():
    """Test that the program handles wrong server address."""
    cmd = [path, "-f", file_correct, "-s", "error", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=15)
    assert returncode == 1


def test_nonexistent_file():
    """Test that the program handles nonexistent file."""
    cmd = [path, "-f", file_nonexistent, "-s", "8.8.8.8", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 1


def test_incorrect_file_format_2_dots():
    """Test that the program handles incorrect file format."""
    cmd = [path, "-f", file_incorrect_dot, "-s", "8.8.8.8", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 2


def test_incorrect_file_format_dash_at_wrong_place():
    """Test that the program handles incorrect file format."""
    cmd = [path, "-f", file_incorrect_dash, "-s", "8.8.8.8", "-p", "5353"]
    output, error, returncode = run_command_with_timeout(cmd, timeout=3)
    assert returncode == 2
