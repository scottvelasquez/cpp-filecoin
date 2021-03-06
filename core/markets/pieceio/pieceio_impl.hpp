/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_MARKETS_PIECEIO_PIECEIO_IMPL_HPP
#define CPP_FILECOIN_CORE_MARKETS_PIECEIO_PIECEIO_IMPL_HPP

#include "markets/pieceio/pieceio.hpp"

#include "common/outcome.hpp"
#include "primitives/cid/cid.hpp"
#include "primitives/piece/piece.hpp"
#include "primitives/sector/sector.hpp"
#include "storage/ipfs/datastore.hpp"
#include "storage/ipld/selector.hpp"

namespace fc::markets::pieceio {
  using Ipld = fc::storage::ipfs::IpfsDatastore;

  class PieceIOImpl : public PieceIO {
   public:
    explicit PieceIOImpl(std::shared_ptr<Ipld> ipld, std::string temp_dir);

    outcome::result<std::pair<CID, UnpaddedPieceSize>> generatePieceCommitment(
        const RegisteredSealProof &registered_proof,
        const CID &payload_cid,
        const Selector &selector) override;

    outcome::result<std::pair<CID, UnpaddedPieceSize>> generatePieceCommitment(
        const RegisteredSealProof &registered_proof,
        const std::string &path) override;

   private:
    std::shared_ptr<Ipld> ipld_;
    std::string temp_dir_;
  };

}  // namespace fc::markets::pieceio

#endif  // CPP_FILECOIN_CORE_MARKETS_PIECEIO_PIECEIO_IMPL_HPP
