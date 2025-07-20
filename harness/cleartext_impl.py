#!/usr/bin/env python3
"""
cleartext_impl.py - Cleartext reference implementation for fetch-by-similarity

This module provides a baseline implementation for the FHE fetch-by-similarity
benchmark that operates on cleartext data.
"""
# Copyright (c) 2025, Amazon Web Services
# All rights reserved.
#
# This software is licensed under the terms of the Apache v2 License.
# See the LICENSE.md file for details.
import sys
from pathlib import Path
import numpy as np
from params import InstanceParams, TOY, LARGE

# The payloads are vectors of 7 int16 numbers in the range [0,4095)
PAYLOAD_DIM = 7

def main():
    """
    Usage: python3 cleartext_impl.py <size> (0-toy/1-small/2-medium/3-large)
    * Reads dataset matrix M, payload vectors, and a query vector v
    * Compute the vector of similarities sim = M*v
    * Extract that payload vectors for rows i for which sim[i]>0.8
    * Sort the extracted vectors and write to disk
    """
    # Parse size argument
    if len(sys.argv) != 2:
        print("Usage: python3 cleartext_impl.py <size> (0-toy/1-small/2-medium/3-large)")
        sys.exit(1)

    size = int(sys.argv[1])
    if size < TOY or size > LARGE:
        raise ValueError(f"Size must be between {TOY} and {LARGE}")

    # Use params.py to get instance parameters
    params = InstanceParams(size)
    dim = params.get_record_dim()

    # Get dataset directory from params
    dataset_dir = params.datadir()
    db = np.fromfile(dataset_dir / "db.bin", dtype=np.float32).reshape(-1, dim)

    # read the query from file, a single record of dimension dim
    v = np.fromfile(dataset_dir / "query.bin", dtype=np.float32)

    # Compute the similarities between the query and all the vectors in db
    sim = db @ v # matrix multiplication

    # read the payloads, n_records vectors of dimension PAYLOAD_DIM=7
    payload_file = dataset_dir / "payloads.bin"
    payloads = np.fromfile(payload_file, dtype=np.int16).reshape(-1, PAYLOAD_DIM)

    # Extract the payload vectors for rows i for which sim[i]>0.8
    extracted_payloads = payloads[sim > 0.8]

    # Sort the extracted payload vectors lexicographically and write to disk
    sorted_ps = extracted_payloads[np.lexsort(extracted_payloads.T[::-1])]
    sorted_ps.tofile(dataset_dir/"expected.bin")


if __name__ == "__main__":
    main()
