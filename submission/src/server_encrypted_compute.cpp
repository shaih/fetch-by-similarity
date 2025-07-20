// server_encrypted_computation.cpp - encrypted fetch-by-similarity
//============================================================================
// Copyright (c) 2025, Amazon Web Services
// All rights reserved.
//
// This software is licensed under the terms of the Apache License v2.
// See the file LICENSE.md for details.
//============================================================================
#include <cassert>

#include "openfhe.h"
#include "cryptocontext-ser.h"  // header files needed for (de)serialization
#include "ciphertext-ser.h"
#include "key/key-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h"

#include "params.h"
#include "utils.h"
#include "slot_replication.h"
#include "running_sums.h"

using namespace lbcrypto;

// A utility function to get one encrypted ciphertext from the dataset. This
// implementation assumes that ciphertexts are just separate files on disk,
// it should be re-written if they are streamed from a remote location.
inline Ciphertext<DCRTPoly> get_ctxt(fs::path ct_name) {
  Ciphertext<DCRTPoly> ct;
  if (!Serial::DeserializeFromFile(ct_name, ct, SerType::BINARY)) {
    throw std::runtime_error("failed to read ciphertext from " + ct_name.string());
  }
  return ct;
}

void log_step(int num, std::string name) {
  auto [timestamp, duration] = getCurrentTimeFormatted();
  std::cout << timestamp << " [server] " << num <<": "<< name << " completed";
  if (duration > 0) {
    std::cout << " (elapsed "<<duration<<"s)";
  }
  std::cout << std::endl;
}

// Matrix-vector product: The matrix rows are stored on disk in batches
// under iodir/batchNNNN/. The equery ciphertext contains the query vector,
// repeatd to fill in all the slots.
std::vector<Ciphertext<DCRTPoly>> mat_vec_mult(fs::path encdir,
                Ciphertext<DCRTPoly> qry, const InstanceParams& prms);

// Compare each slot in the ctxts to the threshold, using a Chebyshev
// approximation of the indicator function chi(x) = (x >= threshold).
// Rather than approximating 0/1 outcome, we scale it to 0/0.5, since we
// will sum up upto eight matches, then multiply by the original thing,
// and need to fit the result to the interval [-1,1].
void compare_to_threshold(std::vector<Ciphertext<DCRTPoly>>& ctxts,
                          double threshold);

// Compare each point in the vectors to the number, using a Chebyshev
// approximation of the function chi(x) = (x == number).
std::vector<Ciphertext<DCRTPoly>> compare_to_number(
    const std::vector<Ciphertext<DCRTPoly>>& ctxts, double number);

// Read the ith payload value of all the records from disk
Ciphertext<DCRTPoly> get_encrypted_payload(fs::path datadir, size_t batch,
                                            size_t idx);

// A SIMD-optimized procedure for computing total sums. The slots in all the
// ciphertexts are viewed as a matrix, and total sums are computed per column.
// The results are returned in a single ciphertext, which is also viewed as
// a matrix with the same number of columns. All the entries of an output
// column contain the total sum of entries from that column in the input.
Ciphertext<DCRTPoly> total_sums(
  const Ciphertext<DCRTPoly>& ct, const InstanceParams& prms);

/*******************************************************************/
int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " instance-size\n";
    std::cout << "  Instance-size: 0-TOY, 1-SMALL, 2-MEDIUM, 3-LARGE\n";
    return 0;
  }
  auto size = static_cast<InstanceSize>(std::stoi(argv[1]));
  InstanceParams prms(size);

  constexpr double threshold = 0.8;

  // Read the crypto context and the public key from disk
  CryptoContext<DCRTPoly> cc;
  if (!Serial::DeserializeFromFile(prms.keydir()/"cc.bin", cc, SerType::BINARY)) {
    throw std::runtime_error("Failed to get CryptoContext from "+prms.keydir().string());
  }
  PublicKey<DCRTPoly> pk;
  if (!Serial::DeserializeFromFile(prms.keydir()/"pk.bin", pk, SerType::BINARY)) {
    throw std::runtime_error("Failed to get public key from "+prms.keydir().string());
  }

  std::ifstream emult_file(prms.keydir()/"mk.bin", std::ios::in | std::ios::binary);
  if (!emult_file.is_open() ||
      !cc->DeserializeEvalMultKey(emult_file, SerType::BINARY)) {
    throw std::runtime_error(
      "Failed to get re-linearization key from " +prms.keydir().string());
  }

  std::ifstream erot_file(prms.keydir()/"rk.bin", std::ios::in | std::ios::binary);
  if (!erot_file.is_open() ||
      !cc->DeserializeEvalAutomorphismKey(erot_file, SerType::BINARY)) {
    throw std::runtime_error(
      "Failed to get rotation keys from " +prms.keydir().string());
  }

  // Read the query vector from disk
  auto q_fname = prms.encdir()/"query.bin";
  Ciphertext<DCRTPoly> eqry;
  if (!Serial::DeserializeFromFile(q_fname,eqry,SerType::BINARY)){
    throw std::runtime_error(
      "failed to read query ciphertext from " + q_fname.string());
  }
  log_step(0, "Loading keys");

  // Matrix-vector multiplication, reading the encrypted matrix from encdir
  auto result = mat_vec_mult(prms.encdir(), eqry, prms);
  log_step(1, "Matrix-vector product");

  // Compare each slot in the results ctxts to the threshold, using a
  // Chebyshev approximation of the indicator function chi(x)=(x>=threshold).
  // Rather than approximating 0/1 outcome, we scale it to 0/sqrt(0.25),
  // since we sum up upto eight matches, then multiply by the original thing,
  // and need to fit the result to the interval [-1,1].
  compare_to_threshold(result, threshold);
  log_step(2, "Compare to threshold");
  // Make a deep copy of the matches, it will be multiplied back into the
  // result after the running-sum procedure
  std::vector<Ciphertext<DCRTPoly>> matches;
  matches.reserve(result.size());
  for (auto& ct : result) {
    matches.push_back(ct->Clone());
  }

  // The "compaction" procedure views the matches vector (which is actually
  // stored in multiple ciphertexts of dimension N_SLOTS) as a matrix with
  // N_COLS columns, and expect no more than eight matches per column. The
  // columns are packed equally-spaced in the ciphertexts, so each ciphertext
  // contains N_SLOTS/N_COLS entries from each column.
  // For example, if we had three ciphertexts with N_SLOTS=8 and N_COLS=4,
  // we would have two entries from each column in each ciphertexts, and the
  // arrnagement is as follows:    [ a1 a2 a3 a4 d1 d2 d3 d4 ]
  //                               [ b1 b2 b3 b4 e1 e2 e3 e4 ]
  //                               [ c1 c2 c3 c4 f1 f2 f3 f4 ]
  // This can be thought of a matrix with i'th column being
  // [ai bi ci di ei fi]^t, we expect no more than 8 ones in each column.

  // Running sums in each column, so the first match will have value 1,
  // the second match will have 2, etc.
  RunningSums rs(cc,prms.getNCols(),RUNNING_SUM_LEVELS,result[0]->GetLevel());
  rs.eval_in_place(result);  // The actual running-sums procedure

  // Multiply by the matches so only the matches (non-zero entries) remain
  for (size_t i = 0; i < result.size(); i++) {
    result[i] = cc->EvalMult(result[i], matches[i]);
  }
  matches.clear();          // not needed anymore
  matches.shrink_to_fit();  // release the memory

  // Contents of slots are now in the range [0,2],
  // subtract one to map them to [-1,1]
  for (auto& ct : result) {
    cc->EvalSubInPlace(ct, 1.0);
  }
  log_step(3, "Running sums");
  // We now get the actual payload data corresponding to the matches. Recall
  // that we expect at most MAX_N_MATCH(=8) matches per column: the 1st is
  // marked by a 1 slot in the result ciphertext, the 2nd by a 2 slot, etc.
  // Recall also that we have PAYLOAD_DIM(=8) of payload slots per record.

  // To get the actual data, we run MAX_N_MATCH(=8) iterations, in the i'th
  // iteration we isolate the PAYLOAD_DIM payload slots of the ith match
  // (i.e., the slot that contains i). We first compute an indicator ctxt
  // with 1 in the slots where result has i, and zero elsewhere (so we have
  // a single 1 per column).

  // Once we compute the i'th indicator, we need to extract the PAYLOAD_DIM
  // payload entries in the columns corresponding to the 1s in this indicator,
  // then move them slots {i*PAYLOAD_DIM,...,(i+1)*PAYLOAD_DIM-1} in their
  // column. We do it in four steps:
  // 1. We multiply each of the PAYLOAD_DIM encrypted payload vectors by the
  //   indicator vector. This yeilds PAYLOAD_DIM vectors with the jth one
  //   containing the jth payload value of the records corresponding to the
  //   1s in the indicator. Each column has at most one non-zero payload
  //   values, all in the same slot.
  // 2. We tile these PAYLOAD_DIM vectors so that the non-zero values appear
  //   in consecutive positions in the column. Since columns in spread
  //   acorss the slots then it means that the PAYLOAD_DIM payload slots
  //   for one record will appear in slots {x, x+N_COLS, x+2*N_COLS,...},
  //   where x is the slot where the indicator has 1 (in that column).
  // 3. We replicate the values across that column, so that it contains
  //   these PAYLOAD_DIM values repeatedly in all the slots in that column.
  // 4. We multiply the result by a mask which is 1 in positions
  //   {i*PAYLOAD_DIM,...,(i+1)*PAYLOAD_DIM-1} in each column and zero
  //   elsewhere.

  Ciphertext<DCRTPoly> accumulator;
  for (int i = 1; i <= prms.getMaxNMatch(); i++) {  // extract i'th match
    double x_i = i / 4.0 - 1.0;  // map from {0,8} to the interval [-1,1]
    auto indicator = compare_to_number(result, x_i);

    // Indicator has 1 in the slots where partial_sums contained i

    // A place holder for the extracted payload, before moving them to
    // their place in the output columns.
    Ciphertext<DCRTPoly> to_replicate;
    for (size_t j = 0; j < PAYLOAD_DIM; j++) {
      // Steps 1 & 2: Multiply by the indicator to get a single payload value
      // per column, then rotate by i*N_COLS to put that value in the next
      // available slot in its column.
      for (size_t k = 0; k < indicator.size(); k++) {
        auto payload_part = get_encrypted_payload(prms.encdir(), k, j);
        // jth row in the k'th matrix

        payload_part = cc->EvalMult(payload_part, indicator[k]);
        if (j == 0 && k == 0) {
          to_replicate = payload_part->Clone();
        } else {
          if (j != 0) {
            payload_part = cc->EvalRotate(payload_part, -j * prms.getNCols());
          }
          to_replicate = cc->EvalAdd(to_replicate, payload_part);
        }
        // We assume that indicator has a single 1 in each output column
        // and all else are zero, so for each index s>N_SLOTS at most
        // one of the values added to to_replicate[s] will be non-zero.
      }
    }

    // Step 3: replicate the values across the column
    // We need to move the (up to) PAYLOAD_DIM non-zero slots in each
    // output column to positions {i*PAYLOAD_DIM,...,(i+1)*PAYLOAD_DIM-1}
    // in that column. This is done by first replicating them so that theu
    // fill the entire column, then multiplying by a mask that zero out
    // everything else, leaving only those positions.
    auto replicated = total_sums(to_replicate, prms);

    // Step 4: multiply by a mask
    std::vector<double> slots(prms.getNSlots(), 0.0);
    for (size_t ell = 0; ell < slots.size(); ell++) {
      int row = ell / prms.getNCols();  // index within column
      if (row >= (i - 1) * PAYLOAD_DIM && row < i * PAYLOAD_DIM) {
        slots[ell] = 1.0;
      }
    }
    auto mask = cc->MakeCKKSPackedPlaintext(slots, 1, replicated->GetLevel());
    auto masked = cc->EvalMult(replicated, mask);

    // Finally, add the payload values to all the other matches in that column
    if (i == 1) {
      accumulator = masked;
    } else {
      accumulator = cc->EvalAdd(accumulator, masked);
    }
  }
  log_step(4, "Output compression");

  // Store the accumulated result back to disk
  std::string out_fname = prms.encdir()/"results.bin";
  if (!Serial::SerializeToFile(out_fname, accumulator, SerType::BINARY)) {
    throw std::runtime_error("Failed to write ciphertext to " + out_fname);
  }
  return 0;
}
/*******************************************************************/
/*******************************************************************/


/*******************************************************************/
// Matrix-vector product: The matrix rows are stored on disk in batches
// under encdir/batchNNNN/. The equery ciphertext contains the query vector
// of dimension RECORD_DIM, repeatd to fill in all the slots.
std::vector<Ciphertext<DCRTPoly>> mat_vec_mult(fs::path encdir,
                Ciphertext<DCRTPoly> qry, const InstanceParams& prms)
{
  CryptoContext<DCRTPoly> cc = qry->GetCryptoContext();

  // The input ciphertext includes a pattern of length RECORD_DIM,
  // repeated N_SLOTS/RECORD_DIM many times to fill all the slot
  auto n_reps = prms.getNSlots() / prms.getRecordDim();
  DFSSlotReplicator replicator(cc, prms.getDegrees(), n_reps);

  auto n_batches = prms.getNCtxts();
  std::vector<Ciphertext<DCRTPoly>> acc(n_batches);  // an accumulator
  size_t i = 0;  // i is the ciphertext index within a batch
  for (auto ct_i = replicator.init(qry); ct_i != nullptr;
       ct_i = replicator.next_replica(), i++) {
    // ct_i has the i'th entry of the query vector in all its slots

    // read a row from each batch, multiply by ct_i and accumulate
    std::stringstream ssi;
    ssi << std::setw(4) << std::setfill('0') << i;
    for (int j = 0; j < n_batches; j++) {  // j is the batch index
      std::stringstream ssj;
      ssj << std::setw(4) << std::setfill('0') << j;

      auto ct_fname = prms.encdir() / 
          ("batch" + ssj.str()) / ("row_" + ssi.str() + ".bin");
      Ciphertext<DCRTPoly> ct = get_ctxt(ct_fname);
      ct = cc->EvalMultNoRelin(ct, ct_i);
      if (i == 0) {  // initialize the accumulator
        acc[j] = ct;
      } else {       // add to the accumulator
        cc->EvalAddInPlace(acc[j], ct);
      }
    }
  }
  // relinearize the accumulators
  for (int j = 0; j < n_batches; j++) {
    cc->RelinearizeInPlace(acc[j]);
  }
  return acc;
}

/*******************************************************************/
// Compare each slot in the ctxts to the threshold, using a Chebyshev
// approximation of the indicator function chi(x) = (x >= threshold). Rather
// than approximating 0/1 outcome, we scale it to 0/sqrt(0.25), since we
// sum up upto eight matches, then multiply by the original thing, and need
// to fit the result to the interval [-1,1].

// A sigmoid-like function. The constant 69 was determined by experiments
constexpr double sigmoid_inscale = 69.0;
double sigmoid(double x, double outscale = 1.0,
               double inscale = sigmoid_inscale) {
  return outscale / (1.0 + std::exp(-(x * inscale)));
}

void compare_to_threshold(std::vector<Ciphertext<DCRTPoly>>& ctxts,
                          double threshold) {
  auto func = [threshold](double x) {
    return sigmoid(x - threshold, /*outscale=*/0.504);
  };
  constexpr size_t degree = 59;  // options are 59, 119, 247
  auto cc = ctxts.front()->GetCryptoContext();
  for (auto& ct : ctxts) {
    ct = cc->EvalChebyshevFunction(func, ct, -1.0, 1.0, degree);
  }
  // We care a lot more about accuracy around zero than around one. If
  // these results are not accurate enough then we can either switch to
  // higher-degree approximation or just suqare the result a few more times.
  // (If we square then we need to change outscale so that after squaring
  // we get sqrt(0.25).)
}

// An impulse-like function
constexpr double impulse_sigma = 0.04;
double impulse(double x, double sigma = impulse_sigma, double scaling = 0.0) {
  if (scaling <= 0.0) {
    scaling = 1.0 / impulse(0.0, sigma, 1.0);
  }
  constexpr double two_pi = 2 * 3.1415926535897932384626433832795;
  double x2 = x * x;
  double sigma2 = sigma * sigma * 2;
  scaling /= (sigma * std::sqrt(two_pi));
  return std::exp(-x2 / sigma2) * scaling;
}

/*******************************************************************/
// Compare each point in the vectors to the number, using a Chebyshev
// approximation of the function chi(x) = (x == number).
std::vector<Ciphertext<DCRTPoly>> compare_to_number(
    const std::vector<Ciphertext<DCRTPoly>>& ctxts, double number) {
  // The outscale is set so to get func(threshold) = 1
  double outscale = 1.0 / impulse(0.0, impulse_sigma, 1.0);
  auto func = [number, outscale](double x) {
    return impulse(x - number, impulse_sigma, outscale);
  };
  constexpr size_t degree = 119;  // options are 59, 119, 247

  auto cc = ctxts[0]->GetCryptoContext();
  std::vector<Ciphertext<DCRTPoly>> results;
  results.reserve(ctxts.size());
  for (auto& ct : ctxts) {
    results.push_back(cc->EvalChebyshevFunction(func, ct, -1.0, 1.0, degree));
  }
  // We care a lot more about accuracy around zero than around one. If
  // these results are not accurate enough then we can either switch to
  // higher-degree approximation or just suqare the result a few more times
  return results;
}

/*******************************************************************/
// A SIMD-optimized procedure for computing total sums. The slots in the
// input ciphertexts are viewed as multiple chunks, each chunk has the same
// number of slots per ciphertexts, interleaved similarly to running sums,
// and the total sums are computed per chunk. The results are returned in
// one ciphertext, where all the slots of a given chunk in that output
// cipehrtext contain the total sum of slots from that input chunk.
Ciphertext<DCRTPoly> total_sums(
  const Ciphertext<DCRTPoly>& ct, const InstanceParams& prms) {
  int period = prms.getNCols() * PAYLOAD_DIM;
  int s = std::log2(prms.getNSlots() / period);
  int r = std::log2(period);
  assert(unsigned(prms.getNSlots()) == 1UL<<(s+r));  // must be a power of two
  auto results = ct->Clone();
  auto cc = results->GetCryptoContext();

  // Total sums inside the vectors, in columns
  for (int i = s - 1; i >= 0; i--) {
    // cyclic rotation of results by 2^{i+r}
    int rot_amount = 1 << (i + r);
    auto tmp = cc->EvalRotate(results, rot_amount);
    // Add tmp back to the results
    cc->EvalAddInPlace(results, tmp);
  }
  return results;
}

// Read the ith payload value in a batch of records from disk
Ciphertext<DCRTPoly> get_encrypted_payload(fs::path datadir, size_t batch,
                                            size_t idx) {
  std::stringstream ssi, ssj;
  ssj << std::setw(4) << std::setfill('0') << batch;
  ssi << std::setw(4) << std::setfill('0') << idx;
  auto dir = datadir / ("batch" + ssj.str());
  auto ct_fname = dir / ("payload_" + ssi.str() + ".bin");

  // read the i'th payload ciphertext from this batch
  Ciphertext<DCRTPoly> result;
  if (!Serial::DeserializeFromFile(ct_fname, result, SerType::BINARY)) {
    throw std::runtime_error("failed to read ciphertext from " + ct_fname.string());
  }
  return result;
}
