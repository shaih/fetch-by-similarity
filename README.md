## Running the "add two numbers" workload

```console
git clone <<reposiroty-URL>> <<path>>
cd <<path>>>
python3 harness/run_submission.py -h # Provide information about command-line options
```
The first time you run `harness/run_submission.py`, it will attempt to pull and build openfhe if it is not already installed, and will then build the submission itself. On subsequent calls it will use the same project without re-buildin git unless the code has changed. An example run is provided below.

```console
$ python3 harness/run_submission.py -h
usage: run_submission.py [-h] [--num_runs NUM_RUNS] [--seed SEED] {0,1,2,3}

Run the fetch-by-similarity FHE benchmark.

positional arguments:
  {0,1,2,3}            Instance size (0-toy/1-small/2-medium/3-large)

options:
  -h, --help           show this help message and exit
  --num_runs NUM_RUNS  Number of times to run steps 4-9 (default: 1)
  --seed SEED          Random seed for dataset and query generation
$
$ python ./harness/run_submission.py 0 --seed 12345 --num_runs 2
[get-openfhe] Found OpenFHE installed at /usr/local/lib/ (use --force to rebuild).
-- FOUND PACKAGE OpenFHE
-- OpenFHE Version: 1.3.0
-- OpenFHE installed as shared libraries: ON
-- OpenFHE include files location: /usr/local/include/openfhe
-- OpenFHE lib files location: /usr/local/lib
-- OpenFHE Native Backend size: 64
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: <<path>>/submission/build
[  4%] Building CXX object CMakeFiles/client_preprocess_dataset.dir/src/client_preprocess_dataset.cpp.o
[...]
[100%] Built target server_encrypted_compute

[harness] Running submission for toy dataset
18:49:49 [harness] 0: Dataset generation completed
18:49:49 [harness] 1: Dataset preprocessing completed (elapsed: 0s)
18:49:51 [harness] 2: Key generation and dataset encryption completed (elapsed: 1s)
         [harness] Keys directory size: 31M
         [harness] Encrypted data directory size: 92M
18:49:51 [harness] 3: Encrypted dataset preprocessing completed (elapsed: 0s)

         [harness] Run 1 of 2
18:49:51 [harness] 4: Query generation completed (elapsed: 0s)
18:49:51 [harness] 5: Query encryption completed (elapsed: 0s)
18:49:51 [server] 0: Loading keys completed
18:49:54 [server] 1: Matrix-vector product completed (elapsed 3s)
18:49:54 [server] 2: Compare to threshold completed
18:49:55 [server] 3: Running sums completed
18:49:57 [server] 4: Output compression completed (elapsed 2s)
18:49:57 [harness] 6: Encrypted computation completed (elapsed: 6s)
18:49:57 [harness] 7: Result decryption and postprocessing completed (elapsed: 0s)
         [harness] PASS (All 12 payload vectors match)

         [harness] Run 2 of 2
18:49:57 [harness] 4: Query generation completed (elapsed: 0s)
18:49:57 [harness] 5: Query encryption completed (elapsed: 0s)
18:49:57 [server] 0: Loading keys completed
18:50:05 [server] 1: Matrix-vector product completed (elapsed 7s)
18:50:06 [server] 2: Compare to threshold completed
18:50:06 [server] 3: Running sums completed
18:50:08 [server] 4: Output compression completed (elapsed 2s)
18:50:08 [harness] 6: Encrypted computation completed (elapsed: 10s)
18:50:08 [harness] 7: Result decryption and postprocessing completed (elapsed: 0s)
         [harness] PASS (All 0 payload vectors match)

All steps completed for toy dataset!
```
