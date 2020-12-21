/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "proofs/proofs.hpp"

#include <random>

#include <gtest/gtest.h>

#include "primitives/piece/piece.hpp"
#include "primitives/piece/piece_data.hpp"
#include "primitives/sector/sector.hpp"
#include "proofs/proof_param_provider.hpp"
#include "storage/filestore/impl/filesystem/filesystem_file.hpp"
#include "testutil/outcome.hpp"
#include "testutil/read_file.hpp"
#include "testutil/storage/base_fs_test.hpp"

using fc::common::Blob;
using fc::crypto::randomness::Randomness;
using fc::primitives::SectorNumber;
using fc::primitives::SectorSize;
using fc::primitives::piece::PaddedPieceSize;
using fc::primitives::piece::PieceData;
using fc::primitives::piece::UnpaddedPieceSize;
using fc::primitives::sector::OnChainSealVerifyInfo;
using fc::primitives::sector::SealVerifyInfo;
using fc::primitives::sector::SectorId;
using fc::primitives::sector::SectorInfo;
using fc::primitives::sector::Ticket;
using fc::primitives::sector::WinningPoStVerifyInfo;
using fc::proofs::ActorId;
using fc::proofs::PieceInfo;
using fc::proofs::PoStCandidate;
using fc::proofs::PrivateSectorInfo;
using fc::proofs::Proofs;
using fc::proofs::PublicSectorInfo;
using fc::proofs::Seed;
using fc::storage::filestore::File;
using fc::storage::filestore::FileSystemFile;
using fc::storage::filestore::Path;

class ProofsTest : public test::BaseFS_Test {
 public:
  ProofsTest() : test::BaseFS_Test("fc_proofs_test") {
    auto res = fc::proofs::ProofParamProvider::readJson(
        "/var/tmp/filecoin-proof-parameters/parameters.json");
    if (!res.has_error()) {
      params = std::move(res.value());
    }
  }

 protected:
  std::vector<fc::proofs::ParamFile> params;
};

/**
 * @given data of sector
 * @when want to seal data and proof post
 * @then success
 */
TEST_F(ProofsTest, Lifecycle) {
  spdlog::info("PROOFS_TEST BASE: {}", base_path);

  ActorId miner_id = 42;
  Randomness randomness{{9, 9, 9}};
  fc::proofs::RegisteredProof seal_proof_type =
      fc::primitives::sector::RegisteredProof::StackedDRG512MiBSeal;
  fc::proofs::RegisteredProof winning_post_proof_type =
      fc::primitives::sector::RegisteredProof::StackedDRG512MiBWinningPoSt;
  SectorNumber sector_num = 42;
  EXPECT_OUTCOME_TRUE(sector_size,
                      fc::primitives::sector::getSectorSize(seal_proof_type));
  spdlog::info("PROOFS_TEST: getParams");
  EXPECT_OUTCOME_TRUE_1(
      fc::proofs::ProofParamProvider::getParams(params, sector_size));

  Ticket ticket{{5, 4, 2}};

  Seed seed{{7, 4, 2}};

  Path sector_cache_dir_path =
      boost::filesystem::unique_path(
          fs::canonical(base_path).append("sector-cache-dir"))
          .string();
  boost::filesystem::create_directory(sector_cache_dir_path);

  Path staged_sector_file =
      boost::filesystem::unique_path(
          fs::canonical(base_path).append("staged-sector-file"))
          .string();
  boost::filesystem::ofstream(staged_sector_file).close();

  Path sealed_sector_file =
      boost::filesystem::unique_path(
          fs::canonical(base_path).append("sealed-sector-file"))
          .string();
  boost::filesystem::ofstream(sealed_sector_file).close();

  Path unseal_output_file_a =
      boost::filesystem::unique_path(
          fs::canonical(base_path).append("unseal-output-file-a"))
          .string();
  boost::filesystem::ofstream(unseal_output_file_a).close();

  Path unseal_output_file_b =
      boost::filesystem::unique_path(
          fs::canonical(base_path).append("unseal-output-file-b"))
          .string();
  boost::filesystem::ofstream(unseal_output_file_b).close();

  Path unseal_output_file_c =
      boost::filesystem::unique_path(
          fs::canonical(base_path).append("unseal-output-file-c"))
          .string();
  boost::filesystem::ofstream(unseal_output_file_c).close();

  Path piece_file_a_path =
      boost::filesystem::unique_path(fs::canonical(base_path).append("piece-a"))
          .string();
  boost::filesystem::ofstream(piece_file_a_path).close();

  spdlog::info("PROOFS_TEST: resize_file 1");
  UnpaddedPieceSize piece_commitment_a_size(
      (uint64_t)PaddedPieceSize(sector_size).unpadded() / 2);
  boost::filesystem::resize_file(piece_file_a_path,
                                 (uint64_t)piece_commitment_a_size);

  spdlog::info("PROOFS_TEST: generatePieceCIDFromFile");
  EXPECT_OUTCOME_TRUE(
      piece_cid_a,
      Proofs::generatePieceCIDFromFile(
          seal_proof_type, piece_file_a_path, piece_commitment_a_size));

  spdlog::info("PROOFS_TEST: resize_file unsealed sector");
  boost::filesystem::resize_file(staged_sector_file,
                                 (uint64_t)piece_commitment_a_size * 2);

  std::vector<PieceInfo> public_pieces = {
      PieceInfo{.size = piece_commitment_a_size.padded(), .cid = piece_cid_a},
      PieceInfo{.size = piece_commitment_a_size.padded(), .cid = piece_cid_a},
  };

  spdlog::info("PROOFS_TEST: generateUnsealedCID");
  EXPECT_OUTCOME_TRUE(
      pregenerated_unsealed_cid,
      Proofs::generateUnsealedCID(seal_proof_type, public_pieces));

  spdlog::info("PROOFS_TEST: sealPreCommitPhase1");
  EXPECT_OUTCOME_TRUE(seal_precommit_phase1_output,
                      Proofs::sealPreCommitPhase1(seal_proof_type,
                                                  sector_cache_dir_path,
                                                  staged_sector_file,
                                                  sealed_sector_file,
                                                  sector_num,
                                                  miner_id,
                                                  ticket,
                                                  public_pieces));

  spdlog::info("PROOFS_TEST: sealPreCommitPhase2");
  EXPECT_OUTCOME_TRUE(sealed_and_unsealed_cid,
                      Proofs::sealPreCommitPhase2(seal_precommit_phase1_output,
                                                  sector_cache_dir_path,
                                                  sealed_sector_file));

  ASSERT_EQ(sealed_and_unsealed_cid.unsealed_cid, pregenerated_unsealed_cid);

  spdlog::info("PROOFS_TEST: sealCommitPhase1");
  // commit the sector
  EXPECT_OUTCOME_TRUE(
      seal_commit_phase1_output,
      Proofs::sealCommitPhase1(seal_proof_type,
                               sealed_and_unsealed_cid.sealed_cid,
                               sealed_and_unsealed_cid.unsealed_cid,
                               sector_cache_dir_path,
                               sealed_sector_file,
                               sector_num,
                               miner_id,
                               ticket,
                               seed,
                               public_pieces))
  spdlog::info("PROOFS_TEST: sealCommitPhase2");
  EXPECT_OUTCOME_TRUE(seal_proof,
                      Proofs::sealCommitPhase2(
                          seal_commit_phase1_output, sector_num, miner_id));

  spdlog::info("PROOFS_TEST: verifySeal");
  EXPECT_OUTCOME_TRUE(isValid,
                      Proofs::verifySeal(SealVerifyInfo{
                          .seal_proof = seal_proof_type,
                          .sector =
                              SectorId{
                                  .miner = miner_id,
                                  .sector = sector_num,
                              },
                          .deals = {},
                          .randomness = ticket,
                          .interactive_randomness = seed,
                          .proof = seal_proof,
                          .sealed_cid = sealed_and_unsealed_cid.sealed_cid,
                          .unsealed_cid = sealed_and_unsealed_cid.unsealed_cid,
                      }));

  ASSERT_TRUE(isValid);

  spdlog::info("PROOFS_TEST: unseal");
  EXPECT_OUTCOME_TRUE_1(Proofs::unseal(seal_proof_type,
                                       sector_cache_dir_path,
                                       sealed_sector_file,
                                       unseal_output_file_a,
                                       sector_num,
                                       miner_id,
                                       ticket,
                                       sealed_and_unsealed_cid.unsealed_cid));

  spdlog::info("PROOFS_TEST: unsealRange 1");
  EXPECT_OUTCOME_TRUE_1(
      Proofs::unsealRange(seal_proof_type,
                          sector_cache_dir_path,
                          sealed_sector_file,
                          unseal_output_file_b,
                          sector_num,
                          miner_id,
                          ticket,
                          sealed_and_unsealed_cid.unsealed_cid,
                          0,
                          (uint64_t)piece_commitment_a_size));

  spdlog::info("PROOFS_TEST: unsealRange");
  EXPECT_OUTCOME_TRUE_1(
      Proofs::unsealRange(seal_proof_type,
                          sector_cache_dir_path,
                          sealed_sector_file,
                          unseal_output_file_c,
                          sector_num,
                          miner_id,
                          ticket,
                          sealed_and_unsealed_cid.unsealed_cid,
                          (uint64_t)piece_commitment_a_size,
                          (uint64_t)piece_commitment_a_size));

  std::vector<PrivateSectorInfo> private_replicas_info = {};
  private_replicas_info.push_back(PrivateSectorInfo{
      .info =
          SectorInfo{
              .registered_proof = winning_post_proof_type,
              .sector = sector_num,
              .sealed_cid = sealed_and_unsealed_cid.sealed_cid,
          },
      .cache_dir_path = sector_cache_dir_path,
      .post_proof_type = winning_post_proof_type,
      .sealed_sector_path = sealed_sector_file,
  });
  auto private_info = Proofs::newSortedPrivateSectorInfo(private_replicas_info);

  std::vector<SectorInfo> proving_set = {SectorInfo{
      .registered_proof = seal_proof_type,
      .sector = sector_num,
      .sealed_cid = sealed_and_unsealed_cid.sealed_cid,
  }};

  spdlog::info("PROOFS_TEST: generateWinningPoStSectorChallenge");
  EXPECT_OUTCOME_TRUE(
      indices_in_proving_set,
      Proofs::generateWinningPoStSectorChallenge(
          winning_post_proof_type, miner_id, randomness, proving_set.size()));

  std::vector<SectorInfo> challenged_sectors;

  for (auto index : indices_in_proving_set) {
    challenged_sectors.push_back(proving_set.at(index));
  }

  spdlog::info("PROOFS_TEST: generateWinningPoSt");
  EXPECT_OUTCOME_TRUE(
      proofs, Proofs::generateWinningPoSt(miner_id, private_info, randomness));

  spdlog::info("PROOFS_TEST: verifyWinningPoSt");
  EXPECT_OUTCOME_TRUE(res,
                      Proofs::verifyWinningPoSt(WinningPoStVerifyInfo{
                          .randomness = randomness,
                          .proofs = proofs,
                          .challenged_sectors = challenged_sectors,
                          .prover = miner_id,
                      }));
  ASSERT_TRUE(res);

  spdlog::info("PROOFS_TEST END");
}
