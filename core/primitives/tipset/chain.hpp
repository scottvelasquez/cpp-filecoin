/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/ptr.hpp"
#include "primitives/tipset/load.hpp"
#include "storage/buffer_map.hpp"

namespace fc::primitives::tipset::chain {
  using KvPtr = std::shared_ptr<storage::PersistentBufferMap>;

  using TsChain = std::map<Height, TsLazy>;

  struct TsBranch;
  using TsBranchPtr = std::shared_ptr<TsBranch>;
  using TsBranchWeak = std::weak_ptr<TsBranch>;
  using TsBranchIter = std::pair<TsBranchPtr, TsChain::iterator>;
  using TsBranchChildren = std::multimap<Height, TsBranchWeak>;

  struct TsBranch {
    /**
     * @return parent if chain size is 1 else valid branch attached to parent
     */
    static TsBranchPtr make(TsChain chain, TsBranchPtr parent = nullptr);
    static outcome::result<TsBranchPtr> make(TsLoadPtr ts_load,
                                             const TipsetKey &key,
                                             TsBranchPtr parent);

    static outcome::result<TsBranchPtr> load(KvPtr kv);
    static outcome::result<TsBranchPtr> create(KvPtr kv,
                                               const TipsetKey &key,
                                               TsLoadPtr ts_load);

    TsChain chain;
    TsBranchPtr parent;
    TsBranchChildren children;
  };

  /**
   * inclusive revert and apply chains
   */
  using Path = std::pair<TsChain, TsChain>;

  outcome::result<std::pair<Path, std::vector<TsBranchPtr>>> update(
      TsBranchPtr branch, TsBranchIter to_it, KvPtr kv = nullptr);

  /**
   * @return valid iterator
   */
  outcome::result<TsBranchIter> find(TsBranchPtr branch,
                                     Height height,
                                     bool allow_less = true);

  outcome::result<BeaconEntry> latestBeacon(TsLoadPtr ts_load,
                                            TsBranchPtr branch,
                                            Height height);
}  // namespace fc::primitives::tipset::chain
