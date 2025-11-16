import pytest
import subprocess
import time
import os
from pathlib import Path

path = str(Path(__file__).resolve().parent.parent / "dns_test")
file_correct = "test_datafile.txt"
file_block_all = "test_datafile_block_all.txt"
file_allow_all = "test_datafile_allow_all.txt"
port = "1053"

def dig_output_match(output):
    import re
    data = {}

    # Status
    status_match = re.search(r"status:\s+(\w+)", output)
    data["status"] = status_match.group(1) if status_match else None

    # IP in ANSWER SECTION
    
    ip_match = re.search(r"ANSWER SECTION:\n.*\sA\s+([\d\.]+)", output)
    if ip_match is None:
        ip_match = re.search(r"ANSWER SECTION:[\s\S]*?\sAAAA\s+([0-9a-fA-F:]+)", output)

    data["ip"] = ip_match.group(1) if ip_match else None

    # Finds WARNING lines
    warnings = [line.strip() for line in output.splitlines() if line.strip().startswith(";; WARNING:")]
    data["warnings"] = warnings

    return data




def test_correct_domain():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube.com"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout
        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NOERROR", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is not None, "No IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_blocked_domain():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "xq1.net"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "REFUSED", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_non_existent_domain():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "dufam.ze.toto.uz.neexistuje.net"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NXDOMAIN", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_blocked_subdomain():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "subdomain.a.a.a.a.a.a.a.a.a.xq1.net"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "REFUSED", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_non_valid_domain_dash():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube-.com"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NXDOMAIN", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_non_valid_domain_unknown_chars():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube_*.com"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NXDOMAIN", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_main_domain():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "."]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NOERROR", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_blocked_with_different_format():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "yAb-AdiMageS.s3.amazOnaws.Com"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "REFUSED", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()


def test_handling_with_different_format():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "YoUtUBE.COm"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NOERROR", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is not None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_correct_AAAA_query():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube.com", "AAAA"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NOERROR", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is not None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_block_all():
    server_cmd = [path, "-f", file_block_all, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube.com"]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "REFUSED", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_allow_all():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube.com", ]
        dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
        output = dig_output.stdout

        print(output)
        parsed = dig_output_match(output)

        assert parsed["status"] == "NOERROR", f"Unexpected status: {parsed['status']}"
        assert parsed["ip"] is not None, "IP found in ANSWER SECTION"
        assert len(parsed["warnings"]) == 0, f"Warnings found: {parsed['warnings']}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()

def test_threads_blocked_all():
    server_cmd = [path, "-f", file_block_all, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube.com"]

        total = 50
        failed = 0

        for i in range(total):
            dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
            output = dig_output.stdout
            parsed = dig_output_match(output)


            if (
                parsed["status"] != "REFUSED"
                or parsed["ip"] is not None
                or len(parsed["warnings"]) != 0
            ):
                failed += 1
                print(f"Failed query {i+1}: {parsed}")

        print(f"\nPassed: {total - failed}, Failed: {failed} / {total}")
        assert failed == 0, f"{failed} queries failed out of {total}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()


def test_threads_allowed_multiple():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "youtube.com"]

        total = 50
        failed = 0

        for i in range(total):
            dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
            output = dig_output.stdout
            parsed = dig_output_match(output)

            if (
                parsed["status"] != "NOERROR"
                or parsed["ip"] is None
                or len(parsed["warnings"]) != 0
            ):
                failed += 1
                print(f"Failed query {i+1}: {parsed}")

        print(f"\n Passed: {total - failed}, Failed: {failed} / {total}")
        assert failed == 0, f"{failed} queries failed out of {total}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server_proc.kill()


def test_threads_nonexisting_domain_multiple():
    server_cmd = [path, "-f", file_correct, "-s", "2001:4860:4860::8888", "-p", port]
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(1)
        dig_cmd = ["dig", "@::1", "-p", port, "nech.sa.test.vydary.com"]

        total = 50
        failed = 0

        for i in range(total):
            dig_output = subprocess.run(dig_cmd, capture_output=True, text=True, timeout=5)
            output = dig_output.stdout
            parsed = dig_output_match(output)
            
            if (
                parsed["status"] != "NXDOMAIN"
                or parsed["ip"] is not None
                or len(parsed["warnings"]) != 0
            ):
                failed += 1
                print(f" Failed query {i+1}: {parsed}")

        print(f"\nPassed: {total - failed}, Failed: {failed} / {total}")
        assert failed == 0, f"{failed} queries failed out of {total}"

    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server_proc.kill()