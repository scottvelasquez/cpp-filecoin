/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <libp2p/outcome/outcome.hpp>
#include <miner/storage_fsm/impl/basic_precommit_policy.hpp>
#include <miner/storage_fsm/impl/events_impl.hpp>
#include <miner/storage_fsm/impl/tipset_cache_impl.hpp>
#include <miner/storage_fsm/tipset_cache.hpp>
#include <random>
#include <sector_storage/stores/impl/index_impl.hpp>
#include <storage/buffer_map.hpp>
#include <storage/in_memory/in_memory_storage.hpp>
#include "sector_storage/impl/manager_impl.hpp"
#include "sector_storage/impl/scheduler_impl.hpp"

#include "api/make.hpp"
#include "miner/storage_fsm/impl/sealing_impl.hpp"

namespace fs = boost::filesystem;

namespace fc {
  struct Config {
    boost::filesystem::path repo_path;

    auto join(const std::string &path) const {
      return (repo_path / path).string();
    }
  };

  outcome::result<Config> readConfig(int argc, char **argv) {
    namespace po = boost::program_options;
    Config config;
    std::string repo_path;
    po::options_description desc("Fuhon node options");
    po::variables_map vm;
    po::store(parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    config.repo_path = repo_path;
    assert(boost::filesystem::exists(config.repo_path));
    std::ifstream config_file{config.join("config.cfg")};
    if (config_file.good()) {
      po::store(po::parse_config_file(config_file, desc), vm);
      po::notify(vm);
    }
    return config;
  }

  outcome::result<void> main(Config &config) {
    return outcome::success();
  }
}  // namespace fc

using fc::primitives::FsStat;
using fc::sector_storage::stores::StorageConfig;

class MyLocalStorage : public fc::sector_storage::stores::LocalStorage {
 public:
  fc::outcome::result<fc::primitives::FsStat> getStat(
      const std::string &path) override {
    return stat;
  }

  fc::outcome::result<fc::sector_storage::stores::StorageConfig> getStorage()
      override {
    return config;
  }

  fc::outcome::result<void> setStorage(
      std::function<void(fc::sector_storage::stores::StorageConfig &)> action)
      override {
    action(config);
    return fc::outcome::success();
  }

  fc::outcome::result<int64_t> getDiskUsage(const std::string &path) override {
    return disk_usage;
  }

  StorageConfig config;
  int64_t disk_usage;
  FsStat stat;
};

int main(int argc, char **argv) {
  using fc::api::Api;
  using fc::api::TipsetKey;
  using fc::common::Buffer;
  using fc::mining::Address;
  using fc::mining::BasicPreCommitPolicy;
  using fc::mining::Counter;
  using fc::mining::Events;
  using fc::mining::EventsImpl;
  using fc::mining::Manager;
  using fc::mining::PreCommitPolicy;
  using fc::mining::Sealing;
  using fc::mining::SealingImpl;
  using fc::mining::TipsetCache;
  using fc::mining::TipsetCacheImpl;
  using fc::sector_storage::ManagerImpl;
  using fc::sector_storage::Scheduler;
  using fc::sector_storage::SchedulerImpl;
  using fc::sector_storage::stores::LocalStorage;
  using fc::sector_storage::stores::LocalStore;
  using fc::sector_storage::stores::LocalStoreImpl;
  using fc::sector_storage::stores::RemoteStore;
  using fc::sector_storage::stores::RemoteStoreImpl;
  using fc::sector_storage::stores::SectorIndex;
  using fc::sector_storage::stores::SectorIndexImpl;
  using fc::storage::BufferMap;
  using fc::storage::InMemoryStorage;
  using fc::vm::actor::builtin::miner::kMaxSectorExpirationExtension;
  using fc::vm::actor::builtin::miner::kWPoStProvingPeriod;

  fc::primitives::RegisteredProof seal_proof_type =
      RegisteredProof::StackedDRG2KiBSeal;

  std::shared_ptr<Api> api = std::make_shared<Api>();

  std::shared_ptr<TipsetCache> tipset_cache = std::make_shared<TipsetCacheImpl>(
      2 * fc::mining::kGlobalChainConfidence, api->ChainGetTipSetByHeight);
  std::shared_ptr<Events> events =
      std::make_shared<EventsImpl>(api, tipset_cache);
  Address miner_address = fc::primitives::address::Address::makeFromId(1000);

  std::shared_ptr<BufferMap> datastore = std::make_shared<InMemoryStorage>();
  std::shared_ptr<Counter> counter =
      std::make_shared<fc::primitives::StoredCounter>(datastore, "/sectors");

  std::shared_ptr<MyLocalStorage> storage = std::make_shared<MyLocalStorage>();
  storage->config =
      StorageConfig{.storage_paths = {"/Users/soramitsu/MyStorage"}};
  storage->stat = FsStat{
      .capacity = 30000,
      .available = 30000,
      .reserved = 0,
  };

  std::shared_ptr<SectorIndex> index = std::make_shared<SectorIndexImpl>();
  std::vector<std::string> urls = {"127.0.0.1"};

  OUTCOME_EXCEPT(local, LocalStoreImpl::newLocalStore(storage, index, urls));
  std::unordered_map<std::string, std::string> auth_headers = {
      {"Authorization", "Bearer here can be your token"}};
  std::shared_ptr<RemoteStore> remote =
      std::make_shared<RemoteStoreImpl>(std::move(local), auth_headers);
  std::shared_ptr<Scheduler> scheduler =
      std::make_shared<SchedulerImpl>(seal_proof_type);
  fc::sector_storage::SealerConfig config{true, true, true, true};

  OUTCOME_EXCEPT(sealer_, ManagerImpl::newManager(remote, scheduler, config));
  std::shared_ptr<Manager> sealer = std::move(sealer_);

  api->StateMinerProvingDeadline =
      [](const Address &,
         const TipsetKey &) -> fc::outcome::result<fc::api::DeadlineInfo> {
    return fc::api::DeadlineInfo::make(0, 1000, 0);
  };

  OUTCOME_EXCEPT(deadline_info,
                 api->StateMinerProvingDeadline(miner_address, TipsetKey{}));
  std::shared_ptr<PreCommitPolicy> policy =
      std::make_shared<BasicPreCommitPolicy>(
          api,
          kMaxSectorExpirationExtension - 2 * kWPoStProvingPeriod,
          deadline_info.period_start % kWPoStProvingPeriod);
  std::shared_ptr<boost::asio::io_context> context =
      std::make_shared<boost::asio::io_context>();

  std::shared_ptr<Sealing> sealing = std::make_shared<SealingImpl>(
      api, events, miner_address, counter, sealer, policy, context);

  fc::common::Blob<2032> some_bytes;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint8_t> dis(0, 255);
  for (size_t i = 0; i < 2032; i++) {
    some_bytes[i] = dis(gen);
  }

  auto path_model = fs::temp_directory_path().append("%%%%%");
  std::string piece_file_a_path =
      boost::filesystem::unique_path(path_model).string();
  boost::filesystem::ofstream piece_file_a(piece_file_a_path);

  fc::mining::UnpaddedPieceSize piece_commitment_a_size(2032);
  for (size_t i = 0; i < piece_commitment_a_size; i++) {
    piece_file_a << some_bytes[i];
  }
  piece_file_a.close();

  fc::mining::PieceData file_a(piece_file_a_path);

  fc::mining::DealInfo deal{
      .deal_id = 1,
      .deal_schedule =
          fc::mining::types::DealSchedule{
              .start_epoch = 0,
              .end_epoch = 0,
          },
  };

  api->ChainHead = [=]() -> fc::outcome::result<fc::api::Tipset> {
    fc::api::BlockHeader block;
    block.miner = miner_address;
    block.height = 1;
    block.ticket = fc::primitives::block::Ticket{
        .bytes = {32, '\0'},
    };
    return fc::api::Tipset::create({block});
  };

  OUTCOME_EXCEPT(
      piece,
      sealing->addPieceToAnySector(piece_commitment_a_size, file_a, deal));

  OUTCOME_EXCEPT(sealing->startPacking(piece.sector));

  context->run();
}
