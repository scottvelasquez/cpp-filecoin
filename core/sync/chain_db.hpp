/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "branches.hpp"
#include "index_db.hpp"
#include "storage/ipfs/datastore.hpp"

#ifndef CPP_FILECOIN_SYNC_CHAIN_DB_HPP
#define CPP_FILECOIN_SYNC_CHAIN_DB_HPP

namespace fc::sync {

  using TipsetCache = LRUCache<TipsetHash, Tipset>;

  using IpfsStoragePtr = std::shared_ptr<storage::ipfs::IpfsDatastore>;

  class ChainDb {
   public:
    ChainDb();

    outcome::result<void> init(KeyValueStoragePtr key_value_storage,
                               IpfsStoragePtr ipld,
                               std::shared_ptr<IndexDb> index_db,
                               const boost::optional<CID> &genesis_cid,
                               bool creating_new_db);

    outcome::result<void> start(HeadCallback on_heads_changed);

    outcome::result<void> stateIsConsistent() const;

    const CID &genesisCID() const;

    const Tipset &genesisTipset() const;

    bool tipsetIsStored(const TipsetHash &hash) const;

    outcome::result<void> getHeads(const HeadCallback &callback);

    outcome::result<TipsetCPtr> getTipsetByHash(const TipsetHash &hash);

    outcome::result<TipsetCPtr> getTipsetByHeight(Height height);

    outcome::result<void> setCurrentHead(const TipsetHash &head);

    using WalkCallback = std::function<void(TipsetCPtr tipset)>;

    outcome::result<void> walkForward(Height from_height,
                                      Height to_height,
                                      const WalkCallback &cb);

    outcome::result<void> walkBackward(const TipsetHash &from,
                                       Height to_height,
                                       const WalkCallback &cb);

    /// returns next unsynced tipset to be loaded, if any
    outcome::result<boost::optional<TipsetCPtr>> storeTipset(
        std::shared_ptr<Tipset> tipset, const TipsetKey &parent);

    outcome::result<boost::optional<TipsetCPtr>> getUnsyncedBottom(
        const TipsetKey &key);

   private:
    std::error_code state_error_;
    KeyValueStoragePtr key_value_storage_;
    IpfsStoragePtr ipld_;
    std::shared_ptr<IndexDb> index_db_;
    std::shared_ptr<Tipset> genesis_tipset_;
    Branches branches_;
    TipsetCache tipset_cache_;
    HeadCallback head_callback_;
    bool started_ = false;
  };

}  // namespace fc::sync

#endif  // CPP_FILECOIN_SYNC_CHAIN_DB_HPP
