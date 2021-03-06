/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/v2/verified_registry/verified_registry_actor.hpp"
#include "vm/actor/builtin/v0/verified_registry/verified_registry_actor_utils.hpp"

namespace fc::vm::actor::builtin::v2::verified_registry {
  namespace Utils = utils::verified_registry;

  // AddVerifier
  //============================================================================

  ACTOR_METHOD_IMPL(AddVerifier) {
    OUTCOME_TRY(Utils::checkDealSize(params.allowance));

    REQUIRE_NO_ERROR_A(verifier,
                       runtime.resolveAddress(params.address),
                       VMExitCode::kErrIllegalState);

    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<State>());
    OUTCOME_TRY(runtime.validateImmediateCallerIs(state.root_key));
    OUTCOME_TRY(Utils::checkAddress<State>(state, verifier));
    OUTCOME_TRYA(
        state,
        runtime.getCurrentActorStateCbor<State>());  // Lotus gas conformance
    OUTCOME_TRY(v0::verified_registry::AddVerifier::addVerifier(
        runtime, state, verifier, params.allowance));
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  // RemoveVerifier
  //============================================================================

  ACTOR_METHOD_IMPL(RemoveVerifier) {
    REQUIRE_NO_ERROR_A(
        verifier, runtime.resolveAddress(params), VMExitCode::kErrIllegalState);

    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<State>());
    OUTCOME_TRY(runtime.validateImmediateCallerIs(state.root_key));
    OUTCOME_TRYA(
        state,
        runtime.getCurrentActorStateCbor<State>());  // Lotus gas conformance

    REQUIRE_NO_ERROR(state.verifiers.remove(verifier),
                     VMExitCode::kErrIllegalState);
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  // AddVerifiedClient
  //============================================================================

  ACTOR_METHOD_IMPL(AddVerifiedClient) {
    OUTCOME_TRY(Utils::checkDealSize(params.allowance));

    REQUIRE_NO_ERROR_A(client,
                       runtime.resolveAddress(params.address),
                       VMExitCode::kErrIllegalState);

    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<State>());
    OUTCOME_TRY(Utils::checkAddress<State>(state, client));
    OUTCOME_TRYA(
        state,
        runtime.getCurrentActorStateCbor<State>());  // Lotus gas conformance
    OUTCOME_TRY(v0::verified_registry::AddVerifiedClient::addClient(
        runtime, state, client, params.allowance));
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  // UseBytes
  //============================================================================

  ACTOR_METHOD_IMPL(UseBytes) {
    OUTCOME_TRY(runtime.validateImmediateCallerIs(kStorageMarketAddress));

    REQUIRE_NO_ERROR_A(client,
                       runtime.resolveAddress(params.address),
                       VMExitCode::kErrIllegalState);

    OUTCOME_TRY(Utils::checkDealSize(params.deal_size));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<State>());

    auto clientCapAssert = [&runtime](bool condition) -> outcome::result<void> {
      return runtime.vm_assert(condition);
    };
    OUTCOME_TRY(v0::verified_registry::UseBytes::useBytes(
        runtime, state, client, params.deal_size, clientCapAssert));
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  // RestoreBytes
  //============================================================================

  ACTOR_METHOD_IMPL(RestoreBytes) {
    OUTCOME_TRY(runtime.validateImmediateCallerIs(kStorageMarketAddress));
    OUTCOME_TRY(Utils::checkDealSize(params.deal_size));

    REQUIRE_NO_ERROR_A(client,
                       runtime.resolveAddress(params.address),
                       VMExitCode::kErrIllegalState);

    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<State>());
    OUTCOME_TRY(Utils::checkAddress<State>(state, client));
    OUTCOME_TRYA(
        state,
        runtime.getCurrentActorStateCbor<State>());  // Lotus gas conformance
    OUTCOME_TRY(v0::verified_registry::RestoreBytes::restoreBytes(
        runtime, state, client, params.deal_size));
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  //============================================================================

  const ActorExports exports{
      exportMethod<Construct>(),
      exportMethod<AddVerifier>(),
      exportMethod<RemoveVerifier>(),
      exportMethod<AddVerifiedClient>(),
      exportMethod<UseBytes>(),
      exportMethod<RestoreBytes>(),
  };
}  // namespace fc::vm::actor::builtin::v2::verified_registry
