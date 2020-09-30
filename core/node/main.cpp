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

#include <libp2p/protocol/common/asio/asio_scheduler.hpp>
#include <libp2p/injector/host_injector.hpp>

#include "storage/keystore/impl/in_memory/in_memory_keystore.hpp"
#include "blockchain/impl/weight_calculator_impl.hpp"
#include "storage/chain/impl/chain_store_impl.hpp"
#include "storage/ipfs/impl/in_memory_datastore.hpp"
#include "storage/mpool/mpool.hpp"
#include "vm/interpreter/impl/interpreter_impl.hpp"
#include "crypto/bls/impl/bls_provider_impl.hpp"
#include "crypto/secp256k1/impl/secp256k1_provider_impl.hpp"
#include "storage/car/car.hpp"
#include "markets/storage/chain_events/impl/chain_events_impl.hpp"
#include "markets/storage/provider/impl/provider_impl.hpp"
#include "storage/filestore/impl/filesystem/filesystem_filestore.hpp"
#include "markets/storage/client/impl/storage_market_client_impl.hpp"

namespace fs = boost::filesystem;

std::promise<fc::CID> manimp;

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

  auto readFile(const std::string &path) {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    assert(file.good());
    Buffer buffer;
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(common::span::string(buffer).data(), buffer.size());
    return buffer;
  }

  template <typename T> using SP = std::shared_ptr<T>;
  using api::Api;
  using boost::asio::io_context;
  using blockchain::weight::WeightCalculatorImpl;
  using storage::blockchain::ChainStoreImpl;
  using vm::interpreter::InterpreterImpl;
  using libp2p::protocol::AsioScheduler;
  using api::Tipset;
  using storage::keystore::InMemoryKeyStore;
  using api::Address;
  using markets::storage::provider::StorageProviderImpl;
  using storage::ipfs::InMemoryDatastore;
  using markets::storage::client::StorageMarketClientImpl;
  using libp2p::Host;
  using storage::InMemoryStorage;
  using api::MinerApi;
  using libp2p::multi::Multiaddress;
  using libp2p::peer::PeerId;
  using api::RegisteredProof;
  using api::TokenAmount;
  using api::UnsignedMessage;
  using markets::pieceio::PieceIOImpl;
  using primitives::piece::UnpaddedPieceSize;
  struct Objects {
    Objects() {
      auto inj{libp2p::injector::makeHostInjector()};
      io = inj.create<std::shared_ptr<io_context>>();
      host = inj.create<std::shared_ptr<Host>>();

      ipld = std::make_shared<InMemoryDatastore>();
      auto roots{storage::car::loadCar(*ipld, readFile(std::string{getenv("HOME")} + "/mygenesis.car")).value()};
      auto genesis{Tipset::load(*ipld, roots).value()};
      auto weighter{std::make_shared<WeightCalculatorImpl>(ipld)};
      auto interpreter{std::make_shared<InterpreterImpl>()};
      chainstore = std::make_shared<ChainStoreImpl>(ipld, weighter, genesis.blks[0], genesis);
      auto mpool{storage::mpool::Mpool::create(ipld, chainstore)};
      auto msgwaiter{storage::blockchain::MsgWaiter::create(ipld, chainstore)};
      auto keystore{std::make_shared<storage::keystore::InMemoryKeyStore>(std::make_shared<crypto::bls::BlsProviderImpl>(), std::make_shared<crypto::secp256k1::Secp256k1ProviderImpl>())};
      auto _api{api::makeImpl(chainstore, weighter, ipld, mpool, interpreter, msgwaiter, nullptr, nullptr, nullptr, keystore)};
      api = std::make_shared<Api>(_api);
      signer = api->WalletImport({api::SignatureType::BLS, common::Blob<32>::fromHex("1914a3112a7a7fb59531ae1052ac572876c1a7b8914ddda6ed1893c78a4daf05").value()}).value();
      sched = std::make_shared<AsioScheduler>(*io, libp2p::protocol::SchedulerConfig{10});

      api->WalletDefaultAddress = [&] { return signer; };
      api->MarketEnsureAvailable = [&](auto &address, auto &wallet, auto &amount, auto &tsk) -> outcome::result<boost::optional<CID>> {
        OUTCOME_TRY(balance, api->StateMarketBalance(address, tsk));
        TokenAmount more{amount - (balance.escrow - balance.locked)};
        if (more <= 0) { return boost::none; }
        UnsignedMessage message;
        message.from = wallet;
        message.to = vm::actor::kStorageMarketAddress;
        message.value = std::move(more);
        message.method = vm::actor::builtin::market::AddBalance::Number;
        message.gas_limit = 10000000;
        OUTCOME_TRYA(message.params, codec::cbor::encode(address));
        OUTCOME_TRY(_message, api->MpoolPushMessage(message));
        return _message.getCid();
      };

      pieceio = std::make_shared<PieceIOImpl>(ipld);
      storageclient = std::make_shared<StorageMarketClientImpl>(host, io, std::make_shared<InMemoryStorage>(), api, pieceio);
      OUTCOME_EXCEPT(storageclient->init());
      storageclient->run();

      mapi = std::make_shared<MinerApi>();
      auto filestore{std::make_shared<storage::filestore::FileSystemFileStore>()};
      auto chain_events{std::make_shared<markets::storage::chain_events::ChainEventsImpl>(api)};
      OUTCOME_EXCEPT(chain_events->init());
      storageprovider = std::make_shared<StorageProviderImpl>( api::RegisteredProof::StackedDRG2KiBSeal, host, io, std::make_shared<InMemoryStorage>(), api, mapi, chain_events, miner, pieceio, filestore);
      OUTCOME_EXCEPT(storageprovider->init());
      OUTCOME_EXCEPT(storageprovider->start());
      OUTCOME_EXCEPT(host->listen(maddr));
      host->start();
    }

    void mine() {
      OUTCOME_EXCEPT(ts, api->ChainHead());
      api::BlockTemplate bt;
      bt.miner = Address::makeFromId(1000);
      bt.parents = ts.cids;
      bt.ticket.emplace();
      bt.height = ts.height + 1;
      bt.messages = api->MpoolPending({}).value();
      auto bm{api->MinerCreateBlock(bt).value()};
      OUTCOME_EXCEPT(api->SyncSubmitBlock(bm));
      sched->schedule(1000, [&] { mine(); }).detach();
    }

    void deal() {
      deal_root = ipld->setCbor(std::string{"sample deal data /.hunter/_Base/130ab68/c914b13/bd99f1b/Install/include/boost/optional/optional.hpp /.hunter/_Base/130ab68/c914b13/bd99f1b/Install/include/boost/optional/optional.hpp /.hunter/_Base/130ab68/c914b13/bd99f1b/Install/include/boost/optional/optional.hpp"}).value();
      auto ab{pieceio->generatePieceCommitment(RegisteredProof::StackedDRG2KiBSeal, deal_root, {}).value()};
      piece = ab.first; piecesize = ab.second;
      OUTCOME_EXCEPT(storageclient->proposeStorageDeal(signer,
        {miner, {}, worker, 2 << 10, {host->getId(), {maddr}}},
        {"manual", deal_root, piece, piecesize},
        60, 60 + 180 * 2880, markets::storage::provider::kDefaultPrice, 0, RegisteredProof::StackedDRG2KiBSeal
      ));
    }

    IpldPtr ipld;
    SP<ChainStoreImpl> chainstore;
    SP<Api> api;
    SP<io_context> io;
    SP<AsioScheduler> sched;
    CID deal_root, piece;
    UnpaddedPieceSize piecesize;
    SP<PieceIOImpl> pieceio;
    Address signer, miner{Address::makeFromId(1000)}, worker{Address::makeFromId(100)};
    SP<Host> host;
    SP<StorageMarketClientImpl> storageclient;
    SP<StorageProviderImpl> storageprovider;
    SP<MinerApi> mapi;
    Multiaddress maddr{Multiaddress::create("/ip4/127.0.0.1/tcp/5975").value()};
  };
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

  fc::Objects obj;
  auto api{obj.api}; auto context{obj.io};

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
  std::vector<std::string> urls = {"http://127.0.0.1"};

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

  std::shared_ptr<Sealing> sealing = std::make_shared<SealingImpl>(
      api, events, miner_address, counter, sealer, policy, context);

  obj.mapi->AddPiece = [&](auto size, auto &path, auto &info) {
    OUTCOME_EXCEPT(piece, sealing->addPieceToAnySector(size, fc::mining::PieceData{path}, info));
    OUTCOME_EXCEPT(sealing->startPacking(piece.sector));
    return fc::outcome::success();
  };

  obj.io->post([&] { obj.mine(); });

  obj.io->post([&] { obj.deal(); });

  std::thread{[&]() {
    auto dc{manimp.get_future().get()};
    spdlog::info("MANIMP");
    OUTCOME_EXCEPT(car, fc::storage::car::makeSelectiveCar(*obj.ipld, {{obj.deal_root, {}}}));
    OUTCOME_EXCEPT(obj.storageprovider->importDataForDeal(dc, car));
  }}.detach();

  context->run();
}
