/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/v0/reward/reward_actor_state.hpp"
#include "common/math/math.hpp"
#include "vm/actor/builtin/v0/reward/reward_actor_calculus.hpp"

namespace fc::vm::actor::builtin::v0::reward {
  using primitives::kChainEpochUndefined;

  State::State(const StoragePower &current_realized_power) {
    effective_network_time = 0;
    effective_baseline_power = kBaselineInitialValueV0;
    this_epoch_reward_smoothed =
        FilterEstimate{.position = kInitialRewardPositionEstimate,
                       .velocity = kInitialRewardVelocityEstimate};
    this_epoch_baseline_power =
        initBaselinePower(kBaselineInitialValueV0, kBaselineExponentV0);
    epoch = kChainEpochUndefined;
    updateToNextEpochWithReward(
        *this, current_realized_power, kBaselineExponentV0);
  }

}  // namespace fc::vm::actor::builtin::v0::reward
