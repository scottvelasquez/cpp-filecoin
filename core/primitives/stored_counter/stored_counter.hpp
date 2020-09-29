/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_PRIMITIVES_STORED_COUNTER_HPP
#define CPP_FILECOIN_CORE_PRIMITIVES_STORED_COUNTER_HPP

#include <mutex>
#include <storage/buffer_map.hpp>
#include "common/buffer.hpp"
#include "common/outcome.hpp"
#include "storage/face/persistent_map.hpp"

namespace fc::primitives {
  using fc::storage::BufferMap;

  class Counter {
   public:
    virtual ~Counter() = default;

    virtual outcome::result<uint64_t> next() = 0;
  };

  class StoredCounter : public Counter {
   public:
    StoredCounter(std::shared_ptr<BufferMap> datastore, std::string key);

    outcome::result<uint64_t> next() override;

   private:
    std::shared_ptr<BufferMap> datastore_;
    Buffer key_;

    std::mutex mutex_;
  };
}  // namespace fc::primitives

#endif  // CPP_FILECOIN_CORE_PRIMITIVES_STORED_COUNTER_HPP
