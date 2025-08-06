# FHE Benchmarking Suite - Fetch-by-Similarity Workload

This repository contains the hardness for the Fetch-by-cosine-similarity workload of the FHE benchmarking suite of [HomomorphicEncrypption.org].
The `main` branch contains a reference implementation of this workload, under the `submission` subdirectory.

Submitters need to clone this reposiroty, create a new branch with a name in the format `<sbumitter>-<date>`, replace the content of the `submission` subdirectory by their own implementation, and push the new branch to this repository.
They also may need to changes or replace the script `scripts/build_task.sh` to account for dependencies and build environment for their submission.

## Running the fetch-by-similarity workload

```console
git clone git@github.com:fhe-benchmarking/fetch-by-similarity.git
cd fetch-by-similarity
python3 harness/run_submission.py -h  # Provide information about command-line options
```

The harness script `harness/run_submission.py` will attempt to build the submission itself, if it is not already built. If already built, it will use the same project without re-building it (unless the code has changed). An example run is provided below.

```console
$ python3 harness/run_submission.py -h
usage: run_submission.py [-h] [--num_runs NUM_RUNS] [--seed SEED] [--count_only] {0,1,2,3}

Run the fetch-by-similarity FHE benchmark.

positional arguments:
  {0,1,2,3}            Instance size (0-toy/1-small/2-medium/3-large)

options:
  -h, --help           show this help message and exit
  --num_runs NUM_RUNS  Number of times to run steps 4-9 (default: 1)
  --seed SEED          Random seed for dataset and query generation
  --count_only         Only count # of matches, do not return payloads
$
$ python ./harness/run_submission.py 0 --seed 12345 --num_runs 2
-- The CXX compiler identification is GNU 13.3.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- FOUND PACKAGE OpenFHE
-- OpenFHE Version: 1.3.0
-- OpenFHE installed as shared libraries: ON
-- OpenFHE include files location: /usr/local/include/openfhe
-- OpenFHE lib files location: /usr/local/lib
-- OpenFHE Native Backend size: 64
-- Configuring done (0.7s)
-- Generating done (0.0s)
-- Build files have been written to: [...]/fetch-by-similarity/submission/build
[  4%] Building CXX object CMakeFiles/client_encode_encrypt_db.dir/src/client_encode_encrypt_db.cpp.o
[...]
[100%] Built target client_encode_encrypt_db

[harness] Running submission for toy dataset
          returning matching payloads
17:57:47 [harness] 1: Dataset generation completed (elapsed: 0.2261s)
17:57:47 [harness] 2: Dataset preprocessing completed (elapsed: 0.0033s)
17:57:48 [harness] 3: Key Generation completed (elapsed: 0.3533s)
17:57:52 [harness] 4: Dataset encoding and encryption completed (elapsed: 4.6505s)
         [harness] Public and evaluation keys size: 30.3M
         [harness] Encrypted database size: 90.3M
17:57:52 [harness] 5: Encrypted dataset preprocessing completed (elapsed: 0.0309s)

         [harness] Run 1 of 2
17:57:53 [harness] 6: Query generation completed (elapsed: 0.2575s)
17:57:53 [harness] 7: Query encryption completed (elapsed: 0.0629s)
         [harness] Encrypted query size: 389.1K
17:57:53 [server] 0: Loading keys completed
17:58:17 [server] 1: Matrix-vector product completed (elapsed 24s)
17:58:18 [server] 2: Compare to threshold completed
17:58:18 [server] 3: Running sums completed
17:58:22 [server] 4: Output compression completed (elapsed 3s)
17:58:22 [harness] 8: Encrypted computation completed (elapsed: 28.9941s)
17:58:22 [harness] 9: Result decryption and postprocessing completed (elapsed: 0.0476s)
         [harness] PASS (All 18 payload vectors match)
[total latency] 34.6261s

         [harness] Run 2 of 2
17:58:22 [harness] 6: Query generation completed (elapsed: 0.5139s)
17:58:22 [harness] 7: Query encryption completed (elapsed: 0.056s)
         [harness] Encrypted query size: 389.1K
17:58:22 [server] 0: Loading keys completed
17:58:31 [server] 1: Matrix-vector product completed (elapsed 8s)
17:58:32 [server] 2: Compare to threshold completed
17:58:32 [server] 3: Running sums completed
17:58:36 [server] 4: Output compression completed (elapsed 3s)
17:58:36 [harness] 8: Encrypted computation completed (elapsed: 13.4689s)
17:58:36 [harness] 9: Result decryption and postprocessing completed (elapsed: 0.0307s)
         [harness] PASS (All 11 payload vectors match)
[total latency] 19.3336s

All steps completed for the toy dataset!
```
