/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FILECOIN_CORE_VM_VERSION_HPP
#define FILECOIN_CORE_VM_VERSION_HPP

#include "primitives/chain_epoch/chain_epoch.hpp"

namespace fc::vm::version {
  using primitives::ChainEpoch;

  /**
   * Enumeration of network upgrades where actor behaviour can change
   */
  enum class NetworkVersion : int64_t {
    kVersion0,
    kVersion1,
    kVersion2,
    kVersion3,
    kVersion4,
    kVersion5,
    kVersion6,
    kVersion7,
    kVersion8,
    kVersion9,
  };

  const NetworkVersion kLatestVersion = NetworkVersion::kVersion9;

  /**
   * Network version end heights
   */
  /** UpgradeBreeze, <= v0 network */
  const ChainEpoch kUpgradeBreezeHeight = -1;

  /** UpgradeSmoke, <= v1 network */
  const ChainEpoch kUpgradeSmokeHeight = -2;

  /** UpgradeIgnition, <= v2 network */
  const ChainEpoch kUpgradeIgnitionHeight = -3;

  /** UpgradeRefuel, <= v3 network */
  const ChainEpoch kUpgradeRefuelHeight = -4;

  /** UpgradeActorsV2, <= v3 network */
  const ChainEpoch kUpgradeActorsV2Height = 3;

  /** UpgradeTape, <= v4 network */
  const ChainEpoch kUpgradeTapeHeight = -5;

  /** UpgradeLiftoff, <= v5 network */
  const ChainEpoch kUpgradeLiftoffHeight = -6;

  /** UpgradeKumquat, <= v5 network */
  const ChainEpoch kUpgradeKumquatHeight = -7;
  const ChainEpoch kUpgradeCalicoHeight = -8;
  const ChainEpoch kUpgradePersianHeight = -9;
  const ChainEpoch kUpgradeOrangeHeight = -10; // 336458

  /**
   * Returns network version for blockchain height
   * @param height - blockchain height
   * @return network version
   */
  inline NetworkVersion getNetworkVersion(const ChainEpoch &height) {
    if (height <= kUpgradeBreezeHeight) return NetworkVersion::kVersion0;
    if (height <= kUpgradeSmokeHeight) return NetworkVersion::kVersion1;
    if (height <= kUpgradeIgnitionHeight) return NetworkVersion::kVersion2;
    if (height <= kUpgradeRefuelHeight) return NetworkVersion::kVersion3;
    if (height <= kUpgradeActorsV2Height) return NetworkVersion::kVersion3;
    if (height <= kUpgradeTapeHeight) return NetworkVersion::kVersion4;
    if (height <= kUpgradeLiftoffHeight) return NetworkVersion::kVersion5;
    if (height <= kUpgradeKumquatHeight) return NetworkVersion::kVersion5;
    if (height <= kUpgradeCalicoHeight) return NetworkVersion::kVersion6;
    if (height <= kUpgradePersianHeight) return NetworkVersion::kVersion7;
    if (height <= kUpgradeOrangeHeight) return NetworkVersion::kVersion8;
    return kLatestVersion;
  }
}  // namespace fc::vm::version

#endif  // FILECOIN_CORE_VM_VERSION_HPP
