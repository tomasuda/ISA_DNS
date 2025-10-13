#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def main():
    tests_dir = Path(__file__).resolve().parent
    test_files = [
        # tests_dir / "input_tests.py",
        tests_dir / "dns_functionality_tests_ipv4.py",
        tests_dir / "dns_functionality_tests_ipv6.py",
    ]

    for test_file in test_files:
        if not test_file.exists():
            print(f"⚠️  File {test_file.name} does not exist — skipping.")
            continue

        print(f"▶  Running {test_file.name}...\n")
        result = subprocess.run(["pytest", str(test_file)], text=True)
        if result.returncode != 0:
            print(f"❌ Tests in {test_file.name} failed.")
            sys.exit(result.returncode)

    print("\n✅ All tests passed successfully.")
    sys.exit(0)

if __name__ == "__main__":
    main()
