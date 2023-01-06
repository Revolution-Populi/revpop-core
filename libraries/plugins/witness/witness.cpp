/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 * Copyright (c) 2018-2022 Revolution Populi Limited, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <graphene/witness/witness.hpp>

#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/commit_reveal_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/thread/thread.hpp>
#include <fc/io/fstream.hpp>

#include <boost/filesystem/path.hpp>

#include <iostream>
#include <chrono>

using namespace graphene::chain;
using namespace graphene::witness_plugin;
using std::string;
using std::vector;

namespace bpo = boost::program_options;

void new_chain_banner( const graphene::chain::database& db )
{
   ilog("\n"
      "********************************\n"
      "*                              *\n"
      "*   ------- NEW CHAIN ------   *\n"
      "*   -  Welcome to RevPop!  -   *\n"
      "*   ------------------------   *\n"
      "*                              *\n"
      "********************************\n"
      "\n");
   if( db.get_slot_at_time( fc::time_point::now() ) > 200 )
   {
      wlog("Your genesis seems to have an old timestamp");
      wlog("Please consider using the --genesis-timestamp option to give your genesis a recent timestamp");
   }
}

void witness_plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   auto default_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("nathan")));
   string witness_id_example = fc::json::to_string(chain::witness_id_type(5));
   command_line_options.add_options()
         ("enable-stale-production", bpo::bool_switch()->notifier([this](bool e){_production_enabled = e;}),
               "Enable block production, even if the chain is stale.")
         ("required-participation", bpo::value<uint32_t>()->default_value(33),
               "Percent of witnesses (0-100) that must be participating in order to produce blocks")
         ("witness-id,w", bpo::value<vector<string>>()->composing()->multitoken(),
               ("ID of witness controlled by this node (e.g. " + witness_id_example +
               ", quotes are required, may specify multiple times)").c_str())
         ("private-key", bpo::value<vector<string>>()->composing()->multitoken()->
          DEFAULT_VALUE_VECTOR(std::make_pair(chain::public_key_type(default_priv_key.get_public_key()),
                graphene::utilities::key_to_wif(default_priv_key))),
                "Tuple of [PublicKey, WIF private key] (may specify multiple times)")
         ("private-key-file", bpo::value<vector<boost::filesystem::path>>()->composing()->multitoken(),
          "Path to a file containing tuples of [PublicKey, WIF private key]."
          " The file has to contain exactly one tuple (i.e. private - public key pair) per line."
          " This option may be specified multiple times, thus multiple files can be provided.")
         ("user-provided-seed", bpo::value<uint64_t>(),
               "A random number that will be used by a pseudo-random number generator as a source of entropy")
         ;
   config_file_options.add(command_line_options);
}

std::string witness_plugin::plugin_name()const
{
   return "witness";
}

void witness_plugin::add_private_key(const std::string& key_id_to_wif_pair_string)
{
   auto key_id_to_wif_pair = graphene::app::dejsonify<std::pair<chain::public_key_type, std::string>>
         (key_id_to_wif_pair_string, 5);
   fc::optional<fc::ecc::private_key> private_key = graphene::utilities::wif_to_key(key_id_to_wif_pair.second);
   if (!private_key)
   {
      // the key isn't in WIF format; see if they are still passing the old native private key format.  This is
      // just here to ease the transition, can be removed soon
      try
      {
         private_key = fc::variant(key_id_to_wif_pair.second, 2).as<fc::ecc::private_key>(1);
      }
      catch (const fc::exception&)
      {
         FC_THROW("Invalid WIF-format private key ${key_string}", ("key_string", key_id_to_wif_pair.second));
      }
   }

   if (_private_keys.find(key_id_to_wif_pair.first) == _private_keys.end())
   {
      ilog("Public Key: ${public}", ("public", key_id_to_wif_pair.first));
      _private_keys[key_id_to_wif_pair.first] = *private_key;
   }
}

void witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   ilog("witness plugin:  plugin_initialize() begin");
   _options = &options;
   LOAD_VALUE_SET(options, "witness-id", _witnesses, chain::witness_id_type)

   if( options.count("private-key") > 0 )
   {
      const std::vector<std::string> key_id_to_wif_pair_strings = options["private-key"].as<std::vector<std::string>>();
      for (const std::string& key_id_to_wif_pair_string : key_id_to_wif_pair_strings)
      {
         add_private_key(key_id_to_wif_pair_string);
      }
   }
   if (options.count("private-key-file") > 0)
   {
      const std::vector<boost::filesystem::path> key_id_to_wif_pair_files =
            options["private-key-file"].as<std::vector<boost::filesystem::path>>();
      for (const boost::filesystem::path& key_id_to_wif_pair_file : key_id_to_wif_pair_files)
      {
         if (fc::exists(key_id_to_wif_pair_file))
         {
            std::string file_content;
            fc::read_file_contents(key_id_to_wif_pair_file, file_content);
            std::istringstream file_content_as_stream(file_content);

            std::string line; // key_id_to_wif_pair_string
            while (std::getline(file_content_as_stream, line))
            {
               add_private_key(line);
            }
         }
         else
         {
            FC_THROW("Failed to load private key file from ${path}", ("path", key_id_to_wif_pair_file.string()));
         }
      }
   }
   if(options.count("required-participation") > 0)
   {
       auto required_participation = options["required-participation"].as<uint32_t>();
       FC_ASSERT(required_participation <= 100);
       _required_witness_participation = options["required-participation"].as<uint32_t>()*GRAPHENE_1_PERCENT;
       if(required_participation < 10)
           wlog("witness plugin: Warning - Low required participation of ${rp}% found", ("rp", required_participation));
       else if(required_participation > 90)
           wlog("witness plugin: Warning - High required participation of ${rp}% found", ("rp", required_participation));
   }
   if(options.count("user-provided-seed") > 0)
   {
      uint64_t user_provided_seed = options["user-provided-seed"].as<uint64_t>();
      uint64_t seed = static_cast<uint64_t>(
         std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^ user_provided_seed;
      gen.seed(seed);
   }
   else
   {
      std::random_device rd;
      gen.seed(rd());
   }

   ilog("witness plugin:  plugin_initialize() end");
} FC_LOG_AND_RETHROW() }

void witness_plugin::plugin_startup()
{ try {
   ilog("witness plugin:  plugin_startup() begin");
   chain::database& d = database();
   if( !_witnesses.empty() )
   {
      ilog("Launching block production for ${n} witnesses.", ("n", _witnesses.size()));
      app().set_block_production(true);
      if( _production_enabled )
      {
         if( d.head_block_num() == 0 )
            new_chain_banner(d);
         _production_skip_flags |= graphene::chain::database::skip_undo_history_check;
      }
      refresh_witness_key_cache();
      d.applied_block.connect( [this]( const chain::signed_block& b )
      {
         refresh_witness_key_cache();
      });
      schedule_production_loop();
   }
   else
   {
      ilog("No witness configured.");
   }

   // RevPop
   check_resources();

   _network_broadcast_api = std::make_shared< app::network_broadcast_api >( std::ref( app() ) );

   database().applied_block.connect([this](const signed_block &b) {
      commit_reveal_operations();
   });

   ilog("witness plugin:  plugin_startup() end");
} FC_CAPTURE_AND_RETHROW() }

void witness_plugin::check_resources() {
   // Every maintenance period check for the witnesses update:
   // old witnesses may lose votes, while new ones can collect them.
   auto& db = database();
   const auto& wit_idx = db.get_index_type<witness_index>();
   const auto& wit_op_idx = wit_idx.indices().get<by_id>();
   std::vector< account_id_type > masters;
   for (const auto& wit_id: _witnesses){
      const auto& wit_itr = wit_op_idx.lower_bound(wit_id);
      if (wit_itr != wit_op_idx.end()) {
         masters.push_back(wit_itr->witness_account);
         _witness_account[wit_itr->witness_account] = wit_id;
      }
   }
   _witness_accounts.swap(masters);
}

void witness_plugin::stop_block_production()
{
   _shutting_down = true;
   
   try {
      if( _block_production_task.valid() )
         _block_production_task.cancel_and_wait(__FUNCTION__);
   } catch(fc::canceled_exception&) {
      //Expected exception. Move along.
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
   }
}

void witness_plugin::refresh_witness_key_cache()
{
   const auto& db = database();
   for( const chain::witness_id_type& wit_id : _witnesses )
   {
      const chain::witness_object* wit_obj = db.find( wit_id );
      if( wit_obj )
         _witness_key_cache[wit_id] = wit_obj->signing_key;
      else
         _witness_key_cache[wit_id] = fc::optional<chain::public_key_type>();
   }
}

void witness_plugin::schedule_production_loop()
{
   if (_shutting_down) return;

   //Schedule for the next second's tick regardless of chain state
   // If we would wait less than 50ms, wait for the whole second.
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_second = 1000000 - (now.time_since_epoch().count() % 1000000);
   if( time_to_next_second < 50000 )      // we must sleep for at least 50ms
       time_to_next_second += 1000000;

   fc::time_point next_wakeup( now + fc::microseconds( time_to_next_second ) );

   _block_production_task = fc::schedule([this]{block_production_loop();},
                                         next_wakeup, "Witness Block Production");
}

void witness_plugin::commit_reveal_operations() {
   if (_witness_accounts.empty() || !_production_enabled )
      return;

   auto& db = database();
   const auto& gpo = db.get_global_properties();
   const auto& dgpo = db.get_dynamic_global_properties();

   // calculate the block number in the current maintenance interval 
   uint32_t last_maintenance_time = dgpo.next_maintenance_time.sec_since_epoch()
                                          - gpo.parameters.maintenance_interval;
   uint32_t maintenance_block_id  = (db.head_block_time().sec_since_epoch() - last_maintenance_time)
                                          / gpo.parameters.block_interval;
   //ilog("A block numer since the maintenance interval interval beginning: ${blc}", ("blc", maintenance_block_id));

   if (maintenance_block_id < static_cast<int32_t>(gpo.parameters.maintenance_skip_slots)) {
      // ---------------------//
      // Maintenance interval //
      //----------------------//
      schedule_commit_reveal();
      return;
   }

   uint64_t total_blocks = gpo.parameters.maintenance_interval / gpo.parameters.block_interval;
   if (maintenance_block_id < total_blocks / 2) {
      // --------------- //
      // Commit interval //
      // --------------- //

      // Even if the required block was skipped, the operation will be executed.
      for (auto& op: _commit_schedule) {
         if (get<0>(op) > maintenance_block_id) return; // block
         if (get<2>(op)) {                   // requires processing
            broadcast_commit(get<1>(op));    // witness
            get<2>(op) = false;
         }
      }
      return;
   }

   // --------------- //
   // Reveal interval //
   // --------------- //

   // Even if the required block was skipped, the operation will be executed.
   for (auto &op : _reveal_schedule) {
      if (get<0>(op) > maintenance_block_id - total_blocks / 2) return; // block
      if (get<2>(op)) {                   // requires processing
         broadcast_reveal(get<1>(op));    // witness
         get<2>(op) = false;
      }
   }

}

void witness_plugin::schedule_commit_reveal() {
   check_resources();
   if (_witness_accounts.empty() || !_production_enabled )
      return;

   auto& db = database();
   const auto& gpo = db.get_global_properties();

   // numer of blocks between 2 maintenances
   int32_t blocks = static_cast<int32_t>(gpo.parameters.maintenance_interval / gpo.parameters.block_interval);

   // Use uniform distribution for the commit operations.
   // For:
   // {
   //    "block_interval": 5,
   //    "maintenance_interval": 300,
   //    "maintenance_skip_slots": 3,
   // }
   // commit interval shoud be from the 3-th to the 29-th blocks
   int32_t skip_blocks = static_cast<int32_t>(gpo.parameters.maintenance_skip_slots);
   std::uniform_int_distribution<int32_t> unidist {skip_blocks, blocks/2 - 1};

   // Use binomial distribution for the reveal operations.
   // For:
   // {
   //    "block_interval": 5,
   //    "maintenance_interval": 300,
   //    "maintenance_skip_slots": 3,
   // }
   // reveal interval shoud be from the 30-th to the 59-th blocks
   std::binomial_distribution<> bindist{blocks/2 - 1, 0.8};

   _commit_schedule.clear();
   _reveal_schedule.clear();
   for (const auto& acc_id: _witness_accounts){
      _commit_schedule.emplace_back(unidist(gen), acc_id, true);
      _reveal_schedule.emplace_back(bindist(gen), acc_id, true);
   }
   sort(_commit_schedule.begin(), _commit_schedule.end());
   sort(_reveal_schedule.begin(), _reveal_schedule.end());

   /*
   // DEBUG
   ilog("Scheduled commits:");
   for (const auto& it: _commit_schedule)
      ilog("    ${blc} -> ${nme}(${acc})", ("blc", get<0>(it))("nme", get<1>(it)(db).name)("acc", get<1>(it)));
   ilog("Scheduled reveals:");
   for (const auto& it: _reveal_schedule)
      ilog("    ${blc} -> ${nme}(${acc})", ("blc", get<0>(it) + blocks/2)("nme", get<1>(it)(db).name)("acc", get<1>(it)));
   */
}

void witness_plugin::broadcast_commit(const chain::account_id_type& acc_id) {
   auto& db = database();
   const auto& dgpo = db.get_dynamic_global_properties();
   const auto& gpo = db.get_global_properties();
   const auto& chain_props = db.get_chain_properties();

   // Prepare the block transaction
   protocol::signed_transaction tx;

   // Generate the bet,
   // 0 is not possible secret value
   std::uniform_int_distribution<uint64_t> dis(1);
   _reveal_value[acc_id] = dis(gen);


   // Search for the commit operation
   const auto &cr_idx = db.get_index_type<commit_reveal_index>();
   const auto &by_cr_acc = cr_idx.indices().get<by_account>();
   auto cr_itr = by_cr_acc.lower_bound(acc_id);

   uint32_t maintenance_time = dgpo.next_maintenance_time.sec_since_epoch();
   uint32_t prev_maintenance_time = maintenance_time - gpo.parameters.maintenance_interval;
   if (cr_itr != by_cr_acc.end() && cr_itr->account == acc_id
      && prev_maintenance_time <= cr_itr->maintenance_time
      && cr_itr->maintenance_time < maintenance_time)
   {
      ilog("[${b}: ${nme}(${acc})] Commit operation for the current maintenance period has already been made, value: ${v}",
            ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())("v", _reveal_value[acc_id]));
      return;
   }

   if (!_witness_key_cache[_witness_account[acc_id]].valid()) {
      ilog("[${b}: ${nme}(${acc})] Can't find a witness key for the commit operation, value: ${v}",
            ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())("v", _reveal_value[acc_id]));
      return;
   }

   // Create the commit operation
   commit_create_operation commit_op;
   commit_op.account = acc_id;
   commit_op.maintenance_time = fc::time_point::now().sec_since_epoch();
   commit_op.witness_key = *_witness_key_cache[_witness_account[acc_id]];
   commit_op.hash = fc::sha512::hash(
      std::to_string(_reveal_value[acc_id]) +
      fc::sha256::hash(
         std::to_string(_reveal_value[acc_id]) +
         fc::sha512::hash(
            std::to_string(db.get_maintenance_seed()) +
            commit_op.witness_key.operator std::string() +
            fc::sha512::hash(
               std::to_string(commit_op.maintenance_time)
            ).str()
         ).str()
      ).str()
   );
   _reveal_hash[acc_id] = commit_op.hash;
   tx.operations.push_back(commit_op);
   ilog("[${b}: ${nme}(${acc})] Commit operation was sent, value: ${v}, hash: ${h} | ${wk}",
         ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())("v", _reveal_value[acc_id])("h", commit_op.hash)("wk", commit_op.witness_key));

   // Broadcast the block transaction
   if (tx.operations.size()){
      tx.validate();
      tx.set_reference_block(dgpo.head_block_id);
      tx.set_expiration(dgpo.time + fc::seconds(30));
      tx.clear_signatures();

      const auto &pkey = get_witness_private_key(*_witness_key_cache[_witness_account[acc_id]]);

      if (pkey.valid()) {
         tx.sign(*pkey, chain_props.chain_id);
         try {
            _network_broadcast_api->broadcast_transaction(tx);
         }
         catch (const fc::exception &e) {
            elog("Caught exception while broadcasting tx ${id}:  ${e}",
                  ("id", tx.id().str())("e", e.to_detail_string()));
            throw;
         }
      }
   }
}

void witness_plugin::broadcast_reveal(const chain::account_id_type& acc_id) {
   auto& db = database();
   const auto& dgpo = db.get_dynamic_global_properties();
   const auto& chain_props = db.get_chain_properties();

   // Prepare the block transaction
   protocol::signed_transaction tx;

   // Search for the corresponding commit operation
   const auto &cr_idx = db.get_index_type<commit_reveal_index>();
   const auto &by_cr_acc = cr_idx.indices().get<by_account>();
   auto cr_itr = by_cr_acc.lower_bound(acc_id);

   if (cr_itr == by_cr_acc.end() || cr_itr->account != acc_id || _reveal_value[acc_id] == 0)
   {
      ilog("[${b}: ${nme}(${acc})] Reveal operation can't find the corresponding commit operation, value: ${v}",
            ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())
            ("v", _reveal_value[acc_id]));
      return;
   }

   if (cr_itr->hash != _reveal_hash[acc_id] || cr_itr->value != 0)
   {
      ilog("[${b}: ${nme}(${acc})] Double reveal operations is prohibited, value: ${v}, hash: ${h}",
            ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())
            ("v", _reveal_value[acc_id])("h", _reveal_hash[acc_id]));
      return;
   }

   if (!_witness_key_cache[_witness_account[acc_id]].valid()) {
      ilog("[${b}: ${nme}(${acc})] Can't find a witness key for the commit operation, value: ${v}",
            ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())
            ("v", _reveal_value[acc_id]));
      return;
   }

   // Create the reveal operation
   reveal_create_operation reveal_op;
   reveal_op.account = acc_id;
   reveal_op.value = _reveal_value[acc_id];
   reveal_op.maintenance_time = fc::time_point::now().sec_since_epoch();
   reveal_op.witness_key = *_witness_key_cache[_witness_account[acc_id]];
   tx.operations.push_back(reveal_op);
   ilog("[${b}: ${nme}(${acc})] Reveal operation was sent, value: ${v}, hash: ${h}",
         ("b", db.head_block_num() + 1)("nme", acc_id(db).name)("acc", acc_id(db).get_id())
         ("v", _reveal_value[acc_id])("h", _reveal_hash[acc_id]));
   _reveal_value[acc_id] = 0;

   // Broadcast the block transaction
   if (tx.operations.size()){
      tx.validate();
      tx.set_reference_block(dgpo.head_block_id);
      tx.set_expiration(dgpo.time + fc::seconds(30));
      tx.clear_signatures();

      const auto &pkey = get_witness_private_key(*_witness_key_cache[_witness_account[acc_id]]);

      if (pkey.valid()) {
         tx.sign(*pkey, chain_props.chain_id);
         try {
            _network_broadcast_api->broadcast_transaction(tx);
         }
         catch (const fc::exception &e) {
            elog("Caught exception while broadcasting tx ${id}:  ${e}",
                  ("id", tx.id().str())("e", e.to_detail_string()));
            throw;
         }
      }
   }
}

fc::optional< fc::ecc::private_key > witness_plugin::get_witness_private_key( const public_key_type& public_key ) const {
   for (const auto &key_pair: _private_keys) {
      if (key_pair.first == public_key) {
         return fc::optional< fc::ecc::private_key >(key_pair.second);
      }
   }
   return fc::optional< fc::ecc::private_key >();
}

block_production_condition::block_production_condition_enum witness_plugin::block_production_loop()
{
   block_production_condition::block_production_condition_enum result;
   fc::limited_mutable_variant_object capture( GRAPHENE_MAX_NESTED_OBJECTS );

   if (_shutting_down) 
   {
      result = block_production_condition::shutdown;
   }
   else
   {
      try
      {
         result = maybe_produce_block(capture);
      }
      catch( const fc::canceled_exception& )
      {
         //We're trying to exit. Go ahead and let this one out.
         throw;
      }
      catch( const fc::exception& e )
      {
         elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
         result = block_production_condition::exception_producing_block;
      }
   }

   switch( result )
   {
      case block_production_condition::produced:
         ilog("Generated block #${n} with ${x} transaction(s) and timestamp ${t} at time ${c}", (capture));
         break;
      case block_production_condition::not_synced:
         ilog("Not producing block because production is disabled until we receive a recent block "
              "(see: --enable-stale-production)");
         break;
      case block_production_condition::not_my_turn:
         break;
      case block_production_condition::not_time_yet:
         break;
      case block_production_condition::no_private_key:
         ilog("Not producing block because I don't have the private key for ${scheduled_key}", (capture) );
         break;
      case block_production_condition::low_participation:
         elog("Not producing block because node appears to be on a minority fork with only ${pct}% witness participation",
               (capture) );
         break;
      case block_production_condition::lag:
         elog("Not producing block because node didn't wake up within 2500ms of the slot time.");
         break;
      case block_production_condition::exception_producing_block:
         elog( "exception producing block" );
         break;
      case block_production_condition::shutdown:
         ilog( "shutdown producing block" );
         return result;
      case block_production_condition::no_network:
         ilog( "not connected to P2P network" );
         return result;
      default:
         elog( "unknown condition ${result} while producing block", ("result", (unsigned char)result) );
         break;
   }

   schedule_production_loop();
   return result;
}

block_production_condition::block_production_condition_enum witness_plugin::maybe_produce_block(
      fc::limited_mutable_variant_object& capture )
{
   chain::database& db = database();
   fc::time_point now_fine = fc::time_point::now();
   fc::time_point_sec now = now_fine + fc::microseconds( 500000 );

   // If the next block production opportunity is in the present or future, we're synced.
   if( !_production_enabled )
   {
      if( db.get_slot_time(1) >= now )
      {
         _production_enabled = true;
         ilog("Blockchain is synchronized, we have a recent block");

         // run scheduling even if we are far from the maintenance
         schedule_commit_reveal();
      }
      else
         return block_production_condition::not_synced;
   }

   // is anyone scheduled to produce now or one second in the future?
   uint32_t slot = db.get_slot_at_time( now );
   if( slot == 0 )
   {
      capture("next_time", db.get_slot_time(1));
      return block_production_condition::not_time_yet;
   }

   //
   // this assert should not fail, because now <= db.head_block_time()
   // should have resulted in slot == 0.
   //
   // if this assert triggers, there is a serious bug in get_slot_at_time()
   // which would result in allowing a later block to have a timestamp
   // less than or equal to the previous block
   //
   assert( now > db.head_block_time() );

   graphene::chain::witness_id_type scheduled_witness = db.get_scheduled_witness( slot );
   // we must control the witness scheduled to produce the next block.
   if( _witnesses.find( scheduled_witness ) == _witnesses.end() )
   {
      capture("scheduled_witness", scheduled_witness);
      return block_production_condition::not_my_turn;
   }

   fc::time_point_sec scheduled_time = db.get_slot_time( slot );
   graphene::chain::public_key_type scheduled_key = *_witness_key_cache[scheduled_witness]; // should be valid
   auto private_key_itr = _private_keys.find( scheduled_key );

   if( private_key_itr == _private_keys.end() )
   {
      capture("scheduled_key", scheduled_key);
      return block_production_condition::no_private_key;
   }

   uint32_t prate = db.witness_participation_rate();
   if( prate < _required_witness_participation )
   {
      capture("pct", uint32_t(100*uint64_t(prate) / GRAPHENE_1_PERCENT));
      return block_production_condition::low_participation;
   }

   if( llabs((scheduled_time - now).count()) > fc::milliseconds( 2500 ).count() )
   {
      capture("scheduled_time", scheduled_time)("now", now);
      return block_production_condition::lag;
   }

   if( p2p_node() == nullptr )
      return block_production_condition::no_network;

   auto block = db.generate_block(
      scheduled_time,
      scheduled_witness,
      private_key_itr->second,
      _production_skip_flags
      );
   capture("n", block.block_num())("t", block.timestamp)("c", now)("x", block.transactions.size());
   fc::async( [this,block](){ p2p_node()->broadcast(net::block_message(block)); } );

   return block_production_condition::produced;
}
