#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

addtest(market_actor_test_v0
    test.cpp
    )
target_link_libraries(market_actor_test_v0
    comm_cid
    ipfs_datastore_in_memory
    market_actor_v0
    piece
    )
