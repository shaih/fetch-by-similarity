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
import numpy as np
import utils
from params import InstanceParams, TOY, LARGE, instance_name

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
    parser.add_argument('--count_only', action='store_true',
                        help='Only count # of matches, do not return payloads')

    args = parser.parse_args()
    size = args.size

    # Use params.py to get instance parameters
    params = InstanceParams(size)


    # Ensure the required directories exist
    utils.ensure_directories(params.rootdir)

    # Verify dependencies and build the submission, if not built already
    subprocess.run(["bash", params.rootdir/"scripts"/"build_task.sh", "./submission"],
                   check=True)

    # The harness scripts are in the 'harness' directory,
    # the executables are in the directory submission/build
    harness_dir = params.rootdir/"harness"
    exec_dir = params.rootdir/"submission"/"build"

    print(f"\n[harness] Running submission for {instance_name(size)} dataset")
    if args.count_only:
        print("          only counting matches")
    else:
        print("          returning matching payloads")

    # 0. Generate the dataset (and centers) using harness/generate_dataset.py

    # Remove and re-create IO directory
    io_dir = params.iodir()
    if io_dir.exists():
        subprocess.run(["rm", "-rf", str(io_dir)], check=True)
    io_dir.mkdir(parents=True)

    if args.seed is not None:
        np.random.seed(args.seed)
        rng = np.random.default_rng(args.seed)
    utils.log_step(0, "Init", True)

    # 1. Client-side: Generate the datasets
    cmd = ["python3", harness_dir/"generate_dataset.py", str(size)]
    if args.seed is not None:  # Use seed if provided
        gendata_seed = rng.integers(0,0x7fffffff)
        cmd.extend(["--seed", str(gendata_seed)])
    subprocess.run(cmd, check=True)
    utils.log_step(1, "Dataset generation")

    # 2. Client-side: Preprocess the dataset using exec_dir/client_preprocess_dataset
    subprocess.run([exec_dir/"client_preprocess_dataset", str(size)], check=True)
    utils.log_step(2, "Dataset preprocessing")

    # 3. Client-side: Generate the cryptographic keys 
    # Note: this does not use the rng seed above, it lets the implementation
    #   handle its own prg needs. It means that even if called with the same
    #   seed multiple times, the keys and ciphertexts will still be different.
    cmd = [exec_dir/"client_key_generation", str(size)]
    if args.count_only:
        cmd.extend(["--count_only"])
    subprocess.run(cmd, check=True)
    utils.log_step(3, "Key Generation")

    # 4. Client-side: Encode and encrypt the dataset
    subprocess.run([exec_dir/"client_encode_encrypt_db", str(size)], check=True)
    utils.log_step(4, "Dataset encoding and encryption")

    # Report size of keys and encrypted data
    utils.log_size(io_dir / "keys", "Public and evaluation keys")
    utils.log_size(io_dir / "encrypted", "Encrypted database")

    # 5. Server-side: Preprocess the encrypted dataset using exec_dir/server_preprocess_dataset
    subprocess.run([exec_dir/"server_preprocess_dataset", str(size)], check=True)
    utils.log_step(5, "Encrypted dataset preprocessing")

    # Run steps 6-11 multiple times if requested
    for run in range(args.num_runs):
        if args.num_runs > 1:
            print(f"\n         [harness] Run {run+1} of {args.num_runs}")

        # 6. Client-side: Generate a new random query using harness/generate_query.py
        cmd = ["python3", harness_dir/"generate_query.py", str(size)]
        if args.seed is not None:
            # Use a different seed for each run but derived from the base seed
            genqry_seed = rng.integers(0,0x7fffffff)
            cmd.extend(["--seed", str(genqry_seed)])
        subprocess.run(cmd, check=True)
        utils.log_step(6, "Query generation")

        # 7. Client-side: Encrypt the query
        subprocess.run([exec_dir/"client_encode_encrypt_query", str(size)], check=True)
        utils.log_step(7, "Query encryption")
        utils.log_size(io_dir / "encrypted" / "query.bin" , "Encrypted query")

        # 8. Server-side: run exec_dir/server_encrypted_compute
        cmd = [exec_dir/"server_encrypted_compute", str(size)]
        if args.count_only:
            cmd.extend(["--count_only"])
        subprocess.run(cmd, check=True)
        utils.log_step(8, "Encrypted computation")

        # 9. Client-side: decrypt and postprocess
        subprocess.run([exec_dir/"client_decrypt_decode", str(size)], check=True)
        cmd = [exec_dir/"client_postprocess", str(size)]
        if args.count_only:
            cmd.extend(["--count_only"])
        subprocess.run(cmd, check=True)
        utils.log_step(9, "Result decryption and postprocessing")

        # 10. Run the plaintext processing in cleartext_impl.py and verify_results
        cmd = ["python3", harness_dir/"cleartext_impl.py", str(size)]
        if args.count_only:
            cmd.extend(["--count_only"])
        subprocess.run(cmd, check=True)

        # 11. Verify results
        expected_file = params.datadir() / "expected.bin"
        result_file = io_dir / "results.bin"

        if not result_file.exists():
            print(f"Error: Result file {result_file} not found")
            sys.exit(1)

        cmd = ["python3", harness_dir/"verify_result.py",
               str(expected_file), str(result_file)]
        if args.count_only:
            cmd.extend(["--count_only"])
        subprocess.run(cmd, check=False)

        # 13. Store measurements
        run_path = params.measuredir() / f"results-{run+1}.json"
        run_path.parent.mkdir(parents=True, exist_ok=True)
        utils.save_run(run_path)

    print(f"\nAll steps completed for the {instance_name(size)} dataset!")

if __name__ == "__main__":
    main()
