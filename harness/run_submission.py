#!/usr/bin/env python3
"""
run_submission.py - run the entire submission process, from build to verify
"""
# Copyright (c) 2025, Amazon Web Services
# All rights reserved.
#
# This software is licensed under the terms of the Apache v2 License.
# See the LICENSE.md file for details.
import sys
import argparse
import subprocess
import time
from datetime import datetime
from pathlib import Path
import numpy as np
from params import InstanceParams, TOY, LARGE, instance_name

def build_submission(script_dir: Path):
    """
    Build the submission, including pulling dependencies as neeed
    """
    # Clone and build OpenFHE if needed
    subprocess.run([script_dir/"get_openfhe.sh"], check=True)
    # CMake build of the submission itself
    subprocess.run([script_dir/"build_task.sh", "./submission"], check=True)

# Global variable to track the last timestamp
_last_timestamp: datetime = None

def log_step(step_num: int, step_name: str):
    """ Helper function to print timestamp after each step with second precision """
    global _last_timestamp
    now = datetime.now()
    # Format with seconds precision
    timestamp = now.strftime("%H:%M:%S")

    # Calculate elapsed time if this isn't the first call
    elapsed_str = ""
    if _last_timestamp is not None:
        elapsed_seconds = (now - _last_timestamp).total_seconds()
        elapsed_str = f" (elapsed: {int(elapsed_seconds)}s)"

    # Update the last timestamp for the next call
    _last_timestamp = now

    print(f"{timestamp} [harness] {step_num}: {step_name} completed{elapsed_str}")

def main():
    """
    Run the entire submission process, from build to verify
    """
    # Parse arguments using argparse
    parser = argparse.ArgumentParser(description='Run the fetch-by-similarity FHE benchmark.')
    parser.add_argument('size', type=int, choices=range(TOY, LARGE+1),
                        help='Instance size (0-toy/1-small/2-medium/3-large)')
    parser.add_argument('--num_runs', type=int, default=1,
                        help='Number of times to run steps 4-9 (default: 1)')
    parser.add_argument('--seed', type=int,
                        help='Random seed for dataset and query generation')

    args = parser.parse_args()
    size = args.size

    # Use params.py to get instance parameters
    params = InstanceParams(size)

    # Check that the current directory has sub-directories
    # 'harness', 'scripts', and 'submission'
    required_dirs = ['harness', 'scripts', 'submission']
    for dir_name in required_dirs:
        if not (params.rootdir / dir_name).exists():
            print(f"Error: Required directory '{dir_name}'",
                  f"not found in {params.rootdir}")
            sys.exit(1)

    # Build the submission if not built already
    build_submission(params.rootdir/"scripts")

    # The harness scripts are in the 'harness' directory,
    # the executables are in the directory submission/build
    harness_dir = params.rootdir/"harness"
    exec_dir = params.rootdir/"submission"/"build"

    print(f"\n[harness] Running submission for {instance_name(size)} dataset")

    # 0. Generate the dataset (and centers) using harness/generate_dataset.py

    # Remove and re-create IO directory
    io_dir = params.iodir()
    if io_dir.exists():
        subprocess.run(["rm", "-rf", str(io_dir)], check=True)
    io_dir.mkdir(parents=True)

    if args.seed is not None:
        np.random.seed(args.seed)
        rng = np.random.default_rng()

    # Generate dataset with seed if provided
    cmd = ["python3", harness_dir/"generate_dataset.py", str(size)]
    if args.seed is not None:
        gendata_seed = rng.integers(0,0x7fffffff)
        cmd.extend(["--seed", str(gendata_seed)])
    subprocess.run(cmd, check=True)
    log_step(0, "Dataset generation")

    # 1. Preprocess the dataset using exec_dir/client_preprocess_dataset
    subprocess.run([exec_dir/"client_preprocess_dataset", str(size)], check=True)
    log_step(1, "Dataset preprocessing")

    # 2. Generate the cryptographic keys and encrypt the dataset
    # Note: this does not use the rng seed above, it lets the implementation
    #   handle its own prg needs. It means that even if called with the same
    #   seed multiple times, the keys and ciphertexts will still be different.
    subprocess.run([exec_dir/"client_key_generation", str(size)], check=True)
    subprocess.run([exec_dir/"client_encode_encrypt_db", str(size)], check=True)

    # Report size of keys and encrypted data
    keys_size = subprocess.run(["du", "-sh", io_dir / "keys"], check=True,
                           capture_output=True, text=True).stdout.split()[0]
    encrypted_size = subprocess.run(["du", "-sh", io_dir / "encrypted"],
                check=True, capture_output=True, text=True).stdout.split()[0]

    log_step(2, "Key generation and dataset encryption")
    print("         [harness] Keys directory size:", keys_size)
    print("         [harness] Encrypted data directory size:", encrypted_size)

    # 3. Preprocess the encrypted dataset using exec_dir/server_preprocess_dataset
    subprocess.run([exec_dir/"server_preprocess_dataset", str(size)], check=True)
    log_step(3, "Encrypted dataset preprocessing")

    # Run steps 4-9 multiple times if requested
    for run in range(args.num_runs):
        if args.num_runs > 1:
            print(f"\n         [harness] Run {run+1} of {args.num_runs}")

        # 4. Generate a new random query using harness/generate_query.py
        cmd = ["python3", harness_dir/"generate_query.py", str(size)]
        if args.seed is not None:
            # Use a different seed for each run but derived from the base seed
            genqry_seed = rng.integers(0,0x7fffffff)
            cmd.extend(["--seed", str(genqry_seed)])
        subprocess.run(cmd, check=True)
        log_step(4, "Query generation")

        # 5. Client side, encrypt the query
        subprocess.run([exec_dir/"client_encode_encrypt_query", str(size)], check=True)
        log_step(5, "Query encryption")

        # 6. Server side, run exec_dir/server_encrypted_compute
        subprocess.run([exec_dir/"server_encrypted_compute", str(size)], check=True)
        log_step(6, "Encrypted computation")
        
        # 7. Client side, decrypt and postprocess
        subprocess.run([exec_dir/"client_decrypt_decode", str(size)], check=True)
        subprocess.run([exec_dir/"client_postprocess", str(size)], check=True)
        log_step(7, "Result decryption and postprocessing")

        # 8. Run the plaintext processing in cleartext_impl.py and verify_results
        subprocess.run(["python3", harness_dir/"cleartext_impl.py", str(size)], check=True)

        # 9. Verify results
        expected_file = params.datadir() / "expected.bin"
        result_file = io_dir / "results.bin"

        if not result_file.exists():
            print(f"Error: Result file {result_file} not found")
            sys.exit(1)

        subprocess.run(["python3", harness_dir/"verify_result.py",
                        str(expected_file), str(result_file)], check=False)

    print(f"\nAll steps completed for {instance_name(size)} dataset!")

if __name__ == "__main__":
    main()
