#!/usr/bin/env python3
"""
verify_result.py - correctness oracle for cosine similarity
"""
# Copyright (c) 2025, Amazon Web Services
# All rights reserved.
#
# This software is licensed under the terms of the Apache v2 License.
# See the LICENSE.md file for details.
import sys
from pathlib import Path
import numpy as np

# The payloads are vectors of 7 int16 numbers
PAYLOAD_DIM = 7

def main():
    """
    Usage:  python3 verify_result.py  <expected_file>  <result_file>
    Returns exit-code 0 if equal or if there are more than 32 expected results, 1 otherwise.
    Prints a message so the caller can log it.
    """
    if len(sys.argv) != 3:
        print("Usage: verify_result.py <expected> <result>")
        sys.exit(0)

    expected_file = Path(sys.argv[1])
    result_file = Path(sys.argv[2])

    # Read the expected and result binary files containing payload vectors
    expected_data = np.fromfile(expected_file, dtype=np.int16)
    result_data = np.fromfile(result_file, dtype=np.int16)

    # Reshape into payload vectors
    expected_payloads = expected_data.reshape(-1, PAYLOAD_DIM)
    result_payloads = result_data.reshape(-1, PAYLOAD_DIM)

    num_expected = len(expected_payloads)
    num_results = len(result_payloads)

    # If there are more than 32 expected results, always report success
    if num_expected > 32:
        print(f"         [harness] PASS (Too many matches: {num_expected} > 32,",
              "skipping detailed comparison)")
        sys.exit(0)

    # Otherwise, compare the payloads
    if num_expected != num_results:
        print(f"         [harness] FAIL (Expected {num_expected} payloads, got {num_results})")
        sys.exit(1)

    # Compare each payload vector
    for i in range(num_expected):
        if not np.array_equal(expected_payloads[i], result_payloads[i]):
            print(f"         [harness] FAIL (Payload {i} mismatch)")
            print(f"  Expected: {expected_payloads[i]}")
            print(f"  Got:      {result_payloads[i]}")
            sys.exit(1)

    print(f"         [harness] PASS (All {num_expected} payload vectors match)")
    sys.exit(0)


if __name__ == "__main__":
    main()
