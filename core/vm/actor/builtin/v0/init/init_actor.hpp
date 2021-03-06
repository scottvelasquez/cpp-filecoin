/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "adt/address_key.hpp"
#include "adt/map.hpp"
#include "vm/actor/actor_method.hpp"

namespace fc::vm::actor::builtin::v0::init {
  /// Init actor state
  struct InitActorState {
    /// Allocate new id address
    outcome::result<Address> addActor(const Address &address);

    adt::Map<uint64_t, adt::AddressKeyer> address_map;
    uint64_t next_id{};
    std::string network_name;
  };
  CBOR_TUPLE(InitActorState, address_map, next_id, network_name)

  struct Construct : ActorMethodBase<1> {
    struct Params {
      std::string network_name;
    };
    ACTOR_METHOD_DECL();
  };
  CBOR_TUPLE(Construct::Params, network_name)

  struct Exec : ActorMethodBase<2> {
    struct Params {
      CodeId code;
      MethodParams params;
    };
    struct Result {
      Address id_address;      // The canonical ID-based address for the actor
      Address robust_address;  // A more expensive but re-org-safe address for
                               // the newly created actor
    };
    ACTOR_METHOD_DECL();

    using CallerAssert = std::function<outcome::result<void>(bool)>;
    using ExecAssert = std::function<bool(const CID &, const CID &)>;

    static outcome::result<Result> execute(Runtime &runtime,
                                           const Params &params,
                                           CallerAssert caller_assert,
                                           ExecAssert exec_assert);

    static outcome::result<void> checkCaller(const Runtime &runtime,
                                             const CodeId &code,
                                             CallerAssert caller_assert,
                                             ExecAssert exec_assert);
    static outcome::result<void> createActor(Runtime &runtime,
                                             const Address &id_address,
                                             const Params &params);
  };
  CBOR_TUPLE(Exec::Params, code, params)
  CBOR_TUPLE(Exec::Result, id_address, robust_address)

  inline bool operator==(const Exec::Result &lhs, const Exec::Result &rhs) {
    return lhs.id_address == rhs.id_address
           && lhs.robust_address == rhs.robust_address;
  }

  extern const ActorExports exports;

}  // namespace fc::vm::actor::builtin::v0::init

namespace fc {
  template <>
  struct Ipld::Visit<vm::actor::builtin::v0::init::InitActorState> {
    template <typename Visitor>
    static void call(vm::actor::builtin::v0::init::InitActorState &state,
                     const Visitor &visit) {
      visit(state.address_map);
    }
  };
}  // namespace fc
