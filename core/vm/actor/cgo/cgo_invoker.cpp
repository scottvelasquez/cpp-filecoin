/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/cgo/cgo_invoker.hpp"
#include "vm/actor/cgo/actors.hpp"

namespace fc::vm::actor::cgo {

  CgoInvoker::CgoInvoker(bool test_vectors) : _test_vectors{test_vectors} {}

  void CgoInvoker::config(
      const StoragePower &min_verified_deal_size,
      const StoragePower &consensus_miner_min_power,
      const std::vector<RegisteredProof> &supported_proofs) {
    ::fc::vm::actor::cgo::config(
        min_verified_deal_size, consensus_miner_min_power, supported_proofs);
  }

  outcome::result<InvocationOutput> CgoInvoker::invoke(
      const Actor &actor,
      Runtime &runtime,
      MethodNumber method,
      const MethodParams &params) {
    return ::fc::vm::actor::cgo::invoke(
        runtime.execution(), runtime.getMessage(), actor.code, method, params);
  }

}  // namespace fc::vm::actor::cgo