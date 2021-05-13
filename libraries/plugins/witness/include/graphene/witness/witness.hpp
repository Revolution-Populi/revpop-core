/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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
#include <graphene/protocol/content_vote.hpp>

#include <fc/thread/future.hpp>
#include <fc/optional.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/base64.hpp>

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
      shutdown = 8
   };
}

class witness_plugin : public graphene::app::plugin, public std::enable_shared_from_this<witness_plugin> {
public:
   ~witness_plugin() { stop_block_production(); }

   std::string plugin_name()const override;

   virtual void plugin_set_program_options(
      boost::program_options::options_description &command_line_options,
      boost::program_options::options_description &config_file_options
      ) override;

   void set_block_production(bool allow) { _production_enabled = allow; }
   void stop_block_production();

   virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
   virtual void plugin_startup() override;
   virtual void plugin_shutdown() override;

   inline const fc::flat_map< chain::witness_id_type, fc::optional<chain::public_key_type> >& get_witness_key_cache()
   { return _witness_key_cache; }

private:
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
   void check_resources();
   bool process_master_operations( const chain::signed_block& b );
   void commit_reveal_operations();
   void schedule_commit_reveal();
   void broadcast_commit(const chain::account_id_type& acc_id);
   void broadcast_reveal(const chain::account_id_type& acc_id);
   fc::optional< fc::ecc::private_key > get_witness_private_key( const chain::account_object& acc ) const;

   fc::api< app::network_broadcast_api > _network_broadcast_api;
   std::shared_ptr< operation_visitor > o_v;
   std::vector< chain::account_id_type > _witness_accounts;
   fc::flat_map< account_id_type, uint64_t > _reveal_value;             // witness-> bid
   // block, witness, requires processing
   std::vector<std::tuple<uint64_t, account_id_type, bool>> _commit_schedule;
   // block, witness, requires processing
   std::vector<std::tuple<uint64_t, account_id_type, bool>> _reveal_schedule;
public:
   fc::optional< fc::ecc::private_key > get_witness_private_key( const public_key_type& public_key ) const;
};

struct operation_visitor
{
   typedef void result_type;

   void operator()( const graphene::chain::content_vote_create_operation& o )
   {
      if (std::find(master_accounts.begin(), master_accounts.end(), o.master_account) == master_accounts.end()){
         return;
      }

      const auto& master_public_key = get_active_public_key( o.master_account );
      const auto& subject_public_key = get_active_public_key( o.subject_account );

      if ( master_public_key.valid() && subject_public_key.valid() ) {
         if (auto shr_plugin = plugin.lock()) {
            const auto &master_private_key = shr_plugin->get_witness_private_key(*master_public_key);
            if (master_private_key.valid()) {
               const auto &opt_content_id = try_decrypt_content_card_id(*master_private_key, *subject_public_key,
                                                                        o.master_content_id);
               if (opt_content_id.valid()) {
                  const auto &content_id = *opt_content_id;

                  if (content_votes.find(o.master_account) == content_votes.end()) {
                     content_votes[o.master_account] = fc::flat_map<content_card_id_type, int>();
                     vote_counters[o.master_account] = 0;
                  }

                  if (content_votes[o.master_account].find(content_id) == content_votes[o.master_account].end()) {
                     content_votes[o.master_account][content_id] = 1;
                  } else {
                     content_votes[o.master_account][content_id] += 1;
                  }
                  vote_counters[o.master_account] += 1;
               }
            }
         } else {
            ilog("Plugin pointer is expired");
         }
      }
   }

   void operator()( const content_vote_remove_operation& o )
   {
   }

   template<typename T>
   void operator()( const T& o )
   {
   }

   fc::optional< public_key_type > get_active_public_key( account_id_type acc_id ) const {
      if (auto shr_plugin = plugin.lock()) {
         auto& db = shr_plugin->database();
         const auto& acc_idx = db.get_index_type<account_index>();
         const auto& acc_op_idx = acc_idx.indices().get<by_id>();
         const auto& acc_itr = acc_op_idx.lower_bound(acc_id);
         if (acc_itr->id == acc_id && acc_itr->active.key_auths.size()){
            return fc::optional< public_key_type >(acc_itr->active.key_auths.begin()->first);
         }
      }
      else {
         ilog("Plugin pointer is expired");
      }
      return fc::optional< public_key_type >();
   }

   void set_master_accounts( const std::vector< account_id_type >& masters ) {
      master_accounts.clear();
      std::copy(masters.begin(), masters.end(), std::back_inserter(master_accounts));
   }

   bool no_master_accounts() {
      return master_accounts.empty();
   }

   fc::optional< content_card_id_type > try_decrypt_content_card_id( const fc::ecc::private_key& mst_private_key,
           const fc::ecc::public_key& sub_public_key, const std::string& enc_msg ) {

       const auto& shared_secret = mst_private_key.get_shared_secret( sub_public_key );

       std::vector<std::string> parts;
       boost::split( parts, enc_msg, boost::is_any_of(":") );
       if (parts.size() == 3) {
           const auto nonce = std::stoul(parts[0]);
           const auto checksum = std::stoul(parts[1]);
           const auto msg = fc::base64_decode(parts[2]);

           fc::sha512::encoder enc;
           fc::raw::pack(enc, nonce);
           fc::raw::pack(enc, shared_secret);
           auto encrypt_key = enc.result();

           uint32_t check = fc::sha256::hash(encrypt_key)._hash[0].value();

           if (check != checksum) {
               ilog("Master node can't decrypt content card id.");
               return fc::optional< content_card_id_type >();
           }

           std::vector<char> enc_content_id;
           std::copy(msg.begin(), msg.end(), std::back_inserter(enc_content_id));
           const auto dec_content_id = fc::aes_decrypt(encrypt_key, enc_content_id);

           std::string content_id_str;
           std::copy(dec_content_id.begin(), dec_content_id.end(), std::back_inserter(content_id_str));
           const auto content_id = content_card_id_type(std::stoi(content_id_str));

           return fc::optional< content_card_id_type >(content_id);
       }
       ilog("Master node can't decrypt content card id.");
       return fc::optional< content_card_id_type >();
   }

   fc::flat_map<account_id_type, int> vote_counters;
   fc::flat_map<account_id_type, fc::flat_map< content_card_id_type, int >> content_votes;
   std::vector< account_id_type > master_accounts;
   std::weak_ptr< witness_plugin > plugin;
};

} } //graphene::witness_plugin
