/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 * Copyright (c) 2018-2023 Revolution Populi Limited, and contributors.
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
#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/app/api.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/protocol/block.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/content_card_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/protocol/transaction.hpp>

#include <fc/thread/future.hpp>
#include <fc/optional.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/base64.hpp>
#include <random>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

namespace graphene { namespace witness_plugin {

using namespace graphene::chain;

struct operation_visitor;

namespace block_production_condition
{
   enum block_production_condition_enum
   {
      produced = 0,
      not_synced = 1,
      not_my_turn = 2,
      not_time_yet = 3,
      no_private_key = 4,
      low_participation = 5,
      lag = 6,
      exception_producing_block = 7,
      shutdown = 8,
      no_network = 9
   };
}

class witness_plugin : public graphene::app::plugin, public std::enable_shared_from_this<witness_plugin> {
public:
   using graphene::app::plugin::plugin;
   ~witness_plugin() override { cleanup(); }

   std::string plugin_name()const override;

   void plugin_set_program_options(
      boost::program_options::options_description &command_line_options,
      boost::program_options::options_description &config_file_options
      ) override;

   void set_block_production(bool allow) { _production_enabled = allow; }
   void stop_block_production();

   void plugin_initialize( const boost::program_options::variables_map& options ) override;
   void plugin_startup() override;
   void plugin_shutdown() override { cleanup(); }

   inline const fc::flat_map< chain::witness_id_type, fc::optional<chain::public_key_type> >& get_witness_key_cache()
   { return _witness_key_cache; }

private:
   void cleanup() { stop_block_production(); }

   void schedule_production_loop();
   block_production_condition::block_production_condition_enum block_production_loop();
   block_production_condition::block_production_condition_enum maybe_produce_block( fc::limited_mutable_variant_object& capture );
   void add_private_key(const std::string& key_id_to_wif_pair_string);

   /// Fetch signing keys of all witnesses in the cache from object database and update the cache accordingly
   void refresh_witness_key_cache();

   boost::program_options::variables_map _options;
   bool _production_enabled = false;
   bool _shutting_down = false;
   uint32_t _required_witness_participation = 33 * GRAPHENE_1_PERCENT;
   uint32_t _production_skip_flags = graphene::chain::database::skip_nothing;

   std::map<chain::public_key_type, fc::ecc::private_key, chain::pubkey_comparator> _private_keys;
   std::set<chain::witness_id_type> _witnesses;
   fc::future<void> _block_production_task;

   /// For tracking signing keys of specified witnesses, only update when applied a block
   fc::flat_map< chain::witness_id_type, fc::optional<chain::public_key_type> > _witness_key_cache;

   /// RevPop
   std::mt19937 gen;
   void check_resources();
   void commit_reveal_operations();
   void schedule_commit_reveal();
   void broadcast_commit(const chain::account_id_type& acc_id);
   void broadcast_reveal(const chain::account_id_type& acc_id);
   fc::optional< fc::ecc::private_key > get_witness_private_key( const public_key_type& public_key ) const;

   fc::api< app::network_broadcast_api > _network_broadcast_api;
   std::vector< chain::account_id_type > _witness_accounts;
   fc::flat_map< account_id_type, witness_id_type > _witness_account;
   fc::flat_map< account_id_type, uint64_t > _reveal_value;             // witness-> bid
   fc::flat_map< account_id_type, std::string > _reveal_hash;           // witness-> hash
   // block, witness, requires processing
   std::vector<std::tuple<uint64_t, account_id_type, bool>> _commit_schedule;
   // block, witness, requires processing
   std::vector<std::tuple<uint64_t, account_id_type, bool>> _reveal_schedule;
};

} } //graphene::witness_plugin
