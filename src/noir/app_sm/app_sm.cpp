#include <noir/app_sm/app_sm.h>

#include <fc/log/logger.hpp>
#include <hoytech-cpp/hoytech/hex.h>
#include <quadrable.h>

namespace noir {

#define DB_APP_STATE "app_state"
#define DB_KEY_HEIGHT "last_block_height"
#define DB_KEY_APPHASH "last_block_apphash"

std::unique_ptr<quadrable::Quadrable> db = std::make_unique<quadrable::Quadrable>();
std::unique_ptr<quadrable::Quadrable::UpdateSet> db_changes = nullptr;
std::unique_ptr<lmdb::txn> txn = nullptr;
std::unique_ptr<lmdb::env> lmdb_env = nullptr;
std::unique_ptr<lmdb::dbi> dbi_app_state = nullptr;

app_sm::app_sm() {
}

void app_sm::plugin_initialize(const variables_map &options) {
  ilog("app_sm init");

  // Open db and create a new one if not exists
  std::string db_dir{getenv("HOME")};
  db_dir += "/.tendermint/data/application.db/";
  if (access(db_dir.c_str(), F_OK)) {
    if (mkdir(db_dir.c_str(), 0755))
      elog("Unable to create directory '${db_dir}': ", ("db_dir", db_dir));
  }
  std::string db_file = db_dir + "data.mdb";
  if (access(db_file.c_str(), F_OK)) {
    ilog("db: create new");
  } else {
    ilog("db: open existing");
  }
  lmdb_env = std::make_unique<lmdb::env>(lmdb::env::create());
  lmdb_env->set_max_dbs(64);
  lmdb_env->set_mapsize(1UL * 1024UL * 1024UL * 1024UL * 1024UL);
  lmdb_env->open(db_dir.c_str(), MDB_CREATE, 0664);
  lmdb_env->reader_check();

  init_app_state();
}

void app_sm::plugin_startup() {
  ilog("app_sm start");
}

void app_sm::plugin_shutdown() {
  ilog("app_sm stop");
}

void app_sm::new_txn() {
  txn = std::make_unique<lmdb::txn>(lmdb::txn::begin(*lmdb_env));
  dbi_app_state = std::make_unique<lmdb::dbi>(lmdb::dbi::open(*txn, DB_APP_STATE, MDB_CREATE));
  db->init(*txn);
}

void app_sm::new_db_height(int64_t height) {
  db->fork(*txn, std::to_string(height));
  db_changes = std::make_unique<quadrable::Quadrable::UpdateSet>(db->change());
}

void app_sm::commit_txn() {
  if (txn) {
    txn->commit();
    txn.reset();
  }
}

void app_sm::apply_db() {
  if (!db_changes || !txn)
    return;
  db_changes->apply(*txn);
  db_changes.reset();
  auto stat = db->stats(*txn);
  dlog("HEAD ${head}: numNode: ${node} numBranch: ${branch} numLeaf: ${leaf}",
      ("head", db->getHead())("node", stat.numNodes)("branch", stat.numBranchNodes)("leaf", stat.numLeafNodes));
}

void app_sm::put_tx(std::string key, std::string value) {
  ilog("app_sm put_tx");
  if (txn) {
    db_changes->put(key, value);
  }
}

std::string app_sm::commit() {
  apply_db();
  std::string apphash = std::string(hoytech::to_hex(db->root(*txn)));
  auto last_block_height = std::stoull(std::string(get_app_state(DB_KEY_HEIGHT)));
  put_app_state(DB_KEY_HEIGHT, std::to_string(last_block_height + 1L));
  put_app_state(DB_KEY_APPHASH, apphash);
  last_height = last_block_height + 1L;
  last_apphash = apphash;
  commit_txn();
  return apphash;
}

void app_sm::init_app_state() {
  new_txn();
  std::string_view v;
  if (!dbi_app_state->get(*txn, DB_KEY_HEIGHT, v)) {
    // Create new app_state
    put_app_state(DB_KEY_HEIGHT, "0");
    put_app_state(DB_KEY_APPHASH, "0000000000000000000000000000000000000000000000000000000000000000");
  }
  last_height = std::stoull(std::string(get_app_state(DB_KEY_HEIGHT)));
  last_apphash = std::string(get_app_state(DB_KEY_APPHASH));

  //assert(last_height == 0 || db->getHeadNodeId(*txn, std::to_string(last_height)) != 0);
  db->checkout(std::to_string(last_height));
  commit_txn();
}

std::string_view app_sm::get_app_state(std::string key) {
  std::string_view v;
  dbi_app_state->get(*txn, key, v);
  return v;
}

void app_sm::put_app_state(std::string key, std::string value) {
  dbi_app_state->put(*txn, key, value);
}

int64_t app_sm::get_last_height() {
  return last_height;
}

std::string& app_sm::get_last_apphash() {
  return last_apphash;
}

}
