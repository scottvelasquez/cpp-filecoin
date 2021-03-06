/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_VM_ACTOR_BUILTIN_V0_SHARED_HPP
#define CPP_FILECOIN_VM_ACTOR_BUILTIN_V0_SHARED_HPP

#include "primitives/address/address.hpp"
#include "vm/actor/builtin/v0/miner/miner_actor.hpp"
#include "vm/runtime/runtime.hpp"

/**
 * Code shared by multiple built-in actors
 */

namespace fc::vm::actor::builtin::v0 {

  using runtime::Runtime;

  /// Quality multiplier for committed capacity (no deals) in a sector
  const uint64_t kQualityBaseMultiplier{10};

  /// Quality multiplier for unverified deals in a sector
  const uint64_t kDealWeightMultiplier{10};

  /// Quality multiplier for verified deals in a sector
  const uint64_t kVerifiedDealWeightMultiplier{100};

  /// Precision used for making QA power calculations
  const uint64_t kSectorQualityPrecision{20};

  /**
   * Get worker address
   * @param runtime
   * @param miner
   * @return
   */
  inline outcome::result<miner::ControlAddresses::Result>
  requestMinerControlAddress(Runtime &runtime, const Address &miner) {
    return runtime.sendM<miner::ControlAddresses>(miner, {}, 0);
  }
}  // namespace fc::vm::actor::builtin::v0

#endif  // CPP_FILECOIN_VM_ACTOR_BUILTIN_V0_SHARED_HPP
