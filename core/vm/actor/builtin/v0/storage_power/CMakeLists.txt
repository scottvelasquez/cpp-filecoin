#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

add_library(storage_power_actor_v0
    storage_power_actor_state.cpp
    storage_power_actor_export.cpp
    policy.cpp
    )
target_link_libraries(storage_power_actor_v0
    actor
    alpha_beta_filter
    block
    balance_table
    const
    hamt
    multimap
    uvarint_key
    )
