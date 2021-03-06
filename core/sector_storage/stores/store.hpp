/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_STORE_HPP
#define CPP_FILECOIN_STORE_HPP

#include <functional>
#include "common/outcome.hpp"
#include "primitives/sector_file/sector_file.hpp"
#include "primitives/types.hpp"
#include "sector_storage/stores/index.hpp"
#include "sector_storage/stores/storage.hpp"

namespace fc::sector_storage::stores {

  using primitives::FsStat;
  using primitives::StorageID;
  using primitives::sector_file::SectorFileType;
  using primitives::sector_file::SectorPaths;

  struct AcquireSectorResponse {
    SectorPaths paths;
    SectorPaths storages;
  };

  enum PathType {
    kStorage,
    kSealing,
  };

  enum AcquireMode {
    kMove,
    kCopy,
  };

  class Store {
   public:
    virtual ~Store() = default;

    virtual outcome::result<AcquireSectorResponse> acquireSector(
        SectorId sector,
        RegisteredSealProof seal_proof_type,
        SectorFileType existing,
        SectorFileType allocate,
        PathType path_type,
        AcquireMode mode) = 0;

    virtual outcome::result<void> remove(SectorId sector,
                                         SectorFileType type) = 0;

    /**
     * @note like remove, but doesn't remove the primary sector copy, nor the
     * last non-primary copy if there no primary copies
     */
    virtual outcome::result<void> removeCopies(SectorId sector,
                                               SectorFileType type) = 0;

    virtual outcome::result<void> moveStorage(
        SectorId sector,
        RegisteredSealProof seal_proof_type,
        SectorFileType types) = 0;

    virtual outcome::result<FsStat> getFsStat(StorageID id) = 0;

    virtual std::shared_ptr<SectorIndex> getSectorIndex() const = 0;
  };

  const std::string kMetaFileName = "sectorstore.json";

  class LocalStore : public Store {
   public:
    virtual outcome::result<void> openPath(const std::string &path) = 0;

    virtual outcome::result<std::vector<primitives::StoragePath>>
    getAccessiblePaths() = 0;

    virtual std::shared_ptr<LocalStorage> getLocalStorage() const = 0;

    virtual outcome::result<std::function<void()>> reserve(
        RegisteredSealProof seal_proof_type,
        SectorFileType file_type,
        const SectorPaths &storages,
        PathType path_type) = 0;
  };

  class RemoteStore : public Store {
   public:
    virtual std::shared_ptr<LocalStore> getLocalStore() const = 0;
  };

}  // namespace fc::sector_storage::stores

#endif  // CPP_FILECOIN_STORE_HPP
