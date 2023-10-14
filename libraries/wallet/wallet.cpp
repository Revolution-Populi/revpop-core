/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <list>

#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <fc/git_revision.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/aes.hpp>

#include <graphene/app/api.hpp>
#include <graphene/app/util.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <graphene/protocol/pts_address.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/utilities/git_revision.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/utilities/words.hpp>
#include <graphene/wallet/wallet.hpp>
#include <graphene/wallet/api_documentation.hpp>
#include "wallet_api_impl.hpp"
#include <graphene/debug_witness/debug_api.hpp>

#include "operation_printer.hpp"
#include <graphene/wallet/reflect_util.hpp>

#define BRAIN_KEY_WORD_COUNT 16
#define RANGE_PROOF_MANTISSA 49 // Minimum mantissa bits to "hide" in the range proof.
                                // If this number is set too low, then for large value
                                // commitments the length of the range proof will hint
                                // strongly at the value amount that is being hidden.

namespace graphene { namespace wallet {
   fc::sha256 signed_message::digest()const
   {
      fc::stringstream to_sign;
      to_sign << message << '\n';
      to_sign << "account=" << meta.account << '\n';
      to_sign << "memokey=" << std::string( meta.memo_key ) << '\n';
      to_sign << "block=" << meta.block << '\n';
      to_sign << "timestamp=" << meta.time;

      return fc::sha256::hash( to_sign.str() );
   }
   vector<brain_key_info> utility::derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys)
   {
      // Safety-check
      FC_ASSERT( number_of_desired_keys >= 1 );

      // Create as many derived owner keys as requested
      vector<brain_key_info> results;
      for (int i = 0; i < number_of_desired_keys; ++i) {
        fc::ecc::private_key priv_key = graphene::wallet::detail::derive_private_key( brain_key, i );

        brain_key_info result;
        result.brain_priv_key = brain_key;
        result.wif_priv_key = key_to_wif( priv_key );
        result.pub_key = priv_key.get_public_key();

        results.push_back(result);
      }

      return results;
   }

   brain_key_info utility::suggest_brain_key()
   {
      brain_key_info result;
      // create a private key for secure entropy
      fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
      fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
      fc::bigint entropy1(sha_entropy1.data(), sha_entropy1.data_size());
      fc::bigint entropy2(sha_entropy2.data(), sha_entropy2.data_size());
      fc::bigint entropy(entropy1);
      entropy <<= 8 * sha_entropy1.data_size();
      entropy += entropy2;
      string brain_key = "";

      for (int i = 0; i < BRAIN_KEY_WORD_COUNT; i++)
      {
         fc::bigint choice = entropy % graphene::words::word_list_size;
         entropy /= graphene::words::word_list_size;
         if (i > 0)
            brain_key += " ";
         brain_key += graphene::words::word_list[choice.to_int64()];
      }

      brain_key = detail::normalize_brain_key(brain_key);
      fc::ecc::private_key priv_key = detail::derive_private_key(brain_key, 0);
      result.brain_priv_key = brain_key;
      result.wif_priv_key = key_to_wif(priv_key);
      result.pub_key = priv_key.get_public_key();
      return result;
   }
}}

namespace graphene { namespace wallet {

wallet_api::wallet_api(const wallet_data& initial_data, fc::api<login_api> rapi)
   : my( std::make_unique<detail::wallet_api_impl>(*this, initial_data, rapi) )
{
}

wallet_api::~wallet_api()
{
}

bool wallet_api::copy_wallet_file(string destination_filename)
{
   return my->copy_wallet_file(destination_filename);
}

optional<signed_block_with_info> wallet_api::get_block(uint32_t num)
{
   return my->_remote_db->get_block(num);
}

uint64_t wallet_api::get_account_count() const
{
   return my->_remote_db->get_account_count();
}

vector<account_object> wallet_api::list_my_accounts()
{
   return vector<account_object>(my->_wallet.my_accounts.begin(), my->_wallet.my_accounts.end());
}

map<string,account_id_type> wallet_api::list_accounts(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_accounts(lowerbound, limit, {});
}

vector<asset> wallet_api::list_account_balances(const string& id)
{
   return my->_remote_db->get_account_balances(id, flat_set<asset_id_type>());
}

vector<extended_asset_object> wallet_api::list_assets(const string& lowerbound, uint32_t limit)const
{
   return my->_remote_db->list_assets( lowerbound, limit );
}

uint64_t wallet_api::get_asset_count()const
{
   return my->_remote_db->get_asset_count();
}

signed_transaction wallet_api::htlc_create( string source, string destination, string amount, string asset_symbol,
         string hash_algorithm, const std::string& preimage_hash, uint32_t preimage_size,
         const uint32_t claim_period_seconds, const std::string& memo, bool broadcast)
{
   return my->htlc_create(source, destination, amount, asset_symbol, hash_algorithm, preimage_hash, preimage_size,
         claim_period_seconds, memo, broadcast);
}

fc::optional<fc::variant> wallet_api::get_htlc(std::string htlc_id) const
{
   fc::optional<htlc_object> optional_obj = my->get_htlc(htlc_id);
   if ( optional_obj.valid() )
   {
      const htlc_object& obj = *optional_obj;
      // convert to formatted variant
      fc::mutable_variant_object transfer;
      const auto& from = my->get_account( obj.transfer.from );
      transfer["from"] = from.name;
      const auto& to = my->get_account( obj.transfer.to );
      transfer["to"] = to.name;
      const auto& asset = my->get_asset( obj.transfer.asset_id );
      transfer["asset"] = asset.symbol;
      transfer["amount"] = graphene::app::uint128_amount_to_string( obj.transfer.amount.value, asset.precision );
      if (obj.memo.valid())
         transfer["memo"] = my->read_memo( *obj.memo );
      class htlc_hash_to_variant_visitor
      {
         public:
         typedef fc::mutable_variant_object result_type;

         result_type operator()(const fc::ripemd160& obj)const
         { return convert("RIPEMD160", obj.str()); }
         result_type operator()(const fc::sha1& obj)const
         { return convert("SHA1", obj.str()); }
         result_type operator()(const fc::sha256& obj)const
         { return convert("SHA256", obj.str()); }
         result_type operator()(const fc::hash160& obj)const
         { return convert("HASH160", obj.str()); }
         private:
         result_type convert(const std::string& type, const std::string& hash)const
         {
            fc::mutable_variant_object ret_val;
            ret_val["hash_algo"] = type;
            ret_val["preimage_hash"] = hash;
            return ret_val;
         }
      };
      static htlc_hash_to_variant_visitor hash_visitor;
      fc::mutable_variant_object htlc_lock = obj.conditions.hash_lock.preimage_hash.visit(hash_visitor);
      htlc_lock["preimage_size"] = obj.conditions.hash_lock.preimage_size;
      fc::mutable_variant_object time_lock;
      time_lock["expiration"] = obj.conditions.time_lock.expiration;
      time_lock["time_left"] = fc::get_approximate_relative_time_string(obj.conditions.time_lock.expiration);
      fc::mutable_variant_object conditions;
      conditions["htlc_lock"] = htlc_lock;
      conditions["time_lock"] = time_lock;
      fc::mutable_variant_object result;
      result["transfer"] = transfer;
      result["conditions"] = conditions;
      return fc::optional<fc::variant>(result);
   }
   return fc::optional<fc::variant>();
}

signed_transaction wallet_api::htlc_redeem( std::string htlc_id, std::string issuer, const std::string& preimage,
      bool broadcast)
{

   return my->htlc_redeem(htlc_id, issuer, std::vector<char>(preimage.begin(), preimage.end()), broadcast);
}

signed_transaction wallet_api::htlc_extend ( std::string htlc_id, std::string issuer, const uint32_t seconds_to_add,
      bool broadcast)
{
   return my->htlc_extend(htlc_id, issuer, seconds_to_add, broadcast);
}

vector<operation_detail> wallet_api::get_account_history(const string& name, uint32_t limit)const
{
   vector<operation_detail> result;

   while( limit > 0 )
   {
      bool skip_first_row = false;
      operation_history_id_type start;
      if( result.size() )
      {
         start = result.back().op.id;
         if( start == operation_history_id_type() ) // no more data
            break;
         start = start + (-1);
         if( start == operation_history_id_type() ) // will return most recent history if
                                                    // directly call remote API with this
         {
            start = start + 1;
            skip_first_row = true;
         }
      }

      uint32_t default_page_size = 100;
      uint32_t page_limit = skip_first_row ? std::min<uint32_t>( default_page_size, limit + 1 )
                                           : std::min<uint32_t>( default_page_size, limit );

      vector<operation_history_object> current = my->_remote_hist->get_account_history(
            name,
            operation_history_id_type(),
            page_limit,
            start );
      bool first_row = true;
      for( auto& o : current )
      {
         if( first_row )
         {
            first_row = false;
            if( skip_first_row )
            {
               continue;
            }
         }
         std::stringstream ss;
         auto memo = o.op.visit(detail::operation_printer(ss, *my, o));
         result.push_back( operation_detail{ memo, ss.str(), o } );
      }

      if( current.size() < page_limit )
         break;

      limit -= current.size();
      if( skip_first_row )
         ++limit;
   }

   return result;
}

vector<operation_detail> wallet_api::get_relative_account_history(
      const string& name,
      uint32_t stop,
      uint32_t limit,
      uint32_t start)const
{
   vector<operation_detail> result;
   auto account_id = get_account(name).get_id();

   const account_object& account = my->get_account(account_id);
   const account_statistics_object& stats = my->get_object(account.statistics);

   if(start == 0)
       start = stats.total_ops;
   else
      start = std::min<uint32_t>(start, stats.total_ops);

   uint32_t default_page_size = 100;
   while( limit > 0 )
   {
      uint32_t page_size = std::min<uint32_t>(default_page_size, limit);
      vector <operation_history_object> current = my->_remote_hist->get_relative_account_history(
            name,
            stop,
            page_size,
            start);
      for (auto &o : current) {
         std::stringstream ss;
         auto memo = o.op.visit(detail::operation_printer(ss, *my, o));
         result.push_back(operation_detail{memo, ss.str(), o});
      }
      if (current.size() < page_size)
         break;
      limit -= page_size;
      start -= page_size;
      if( start == 0 ) break;
   }
   return result;
}

account_history_operation_detail wallet_api::get_account_history_by_operations(
      const string& name,
      const flat_set<uint16_t>& operation_types,
      uint32_t start,
      uint32_t limit)
{
    account_history_operation_detail result;

    const auto& account = my->get_account(name);
    const auto& stats = my->get_object(account.statistics);

    // sequence of account_transaction_history_object start with 1
    start = start == 0 ? 1 : start;

    if (start <= stats.removed_ops) {
        start = stats.removed_ops;
        result.total_count =stats.removed_ops;
    }

    uint32_t default_page_size = 100;
    while (limit > 0 && start <= stats.total_ops) {
        uint32_t min_limit = std::min(default_page_size, limit);
        auto current = my->_remote_hist->get_account_history_by_operations(name, operation_types, start, min_limit);
        auto his_rend = current.operation_history_objs.rend();
        for( auto it = current.operation_history_objs.rbegin(); it != his_rend; ++it )
        {
            auto& obj = *it;
            std::stringstream ss;
            auto memo = obj.op.visit(detail::operation_printer(ss, *my, obj));

            transaction_id_type transaction_id;
            auto block = get_block(obj.block_num);
            if (block.valid() && obj.trx_in_block < block->transaction_ids.size()) {
                transaction_id = block->transaction_ids[obj.trx_in_block];
            }
            result.details.push_back(operation_detail_ex{memo, ss.str(), obj, transaction_id});
        }
        result.result_count += current.operation_history_objs.size();
        result.total_count += current.total_count;

        start += current.total_count > 0 ? current.total_count : min_limit;
        limit -= current.operation_history_objs.size();
    }

    return result;
}

full_account wallet_api::get_full_account( const string& name_or_id)
{
    return my->_remote_db->get_full_accounts({name_or_id}, false)[name_or_id];
}

vector<bucket_object> wallet_api::get_market_history(
      string symbol1,
      string symbol2,
      uint32_t bucket,
      fc::time_point_sec start,
      fc::time_point_sec end )const
{
   return my->_remote_hist->get_market_history( symbol1, symbol2, bucket, start, end );
}

vector<limit_order_object> wallet_api::get_account_limit_orders(
      const string& name_or_id,
      const string &base,
      const string &quote,
      uint32_t limit,
      optional<limit_order_id_type> ostart_id,
      optional<price> ostart_price)
{
   return my->_remote_db->get_account_limit_orders(name_or_id, base, quote, limit, ostart_id, ostart_price);
}

vector<limit_order_object> wallet_api::get_limit_orders(std::string a, std::string b, uint32_t limit)const
{
   return my->_remote_db->get_limit_orders(a, b, limit);
}

vector<call_order_object> wallet_api::get_call_orders(std::string a, uint32_t limit)const
{
   return my->_remote_db->get_call_orders(a, limit);
}

vector<force_settlement_object> wallet_api::get_settle_orders(std::string a, uint32_t limit)const
{
   return my->_remote_db->get_settle_orders(a, limit);
}

brain_key_info wallet_api::suggest_brain_key()const
{
   return graphene::wallet::utility::suggest_brain_key();
}

vector<brain_key_info> wallet_api::derive_owner_keys_from_brain_key(
      string brain_key,
      int number_of_desired_keys) const
{
   return graphene::wallet::utility::derive_owner_keys_from_brain_key(brain_key, number_of_desired_keys);
}

bool wallet_api::is_public_key_registered(string public_key) const
{
   bool is_known = my->_remote_db->is_public_key_registered(public_key);
   return is_known;
}


string wallet_api::serialize_transaction( signed_transaction tx )const
{
   return fc::to_hex(fc::raw::pack(tx));
}

variant wallet_api::get_object( object_id_type id ) const
{
   return my->_remote_db->get_objects({id}, {});
}

string wallet_api::get_wallet_filename() const
{
   return my->get_wallet_filename();
}

transaction_handle_type wallet_api::begin_builder_transaction()
{
   return my->begin_builder_transaction();
}

void wallet_api::add_operation_to_builder_transaction(
      transaction_handle_type transaction_handle,
      const operation& op)
{
   my->add_operation_to_builder_transaction(transaction_handle, op);
}

void wallet_api::replace_operation_in_builder_transaction(
      transaction_handle_type handle,
      unsigned operation_index,
      const operation& new_op)
{
   my->replace_operation_in_builder_transaction(handle, operation_index, new_op);
}

asset wallet_api::set_fees_on_builder_transaction(transaction_handle_type handle, string fee_asset)
{
   return my->set_fees_on_builder_transaction(handle, fee_asset);
}

transaction wallet_api::preview_builder_transaction(transaction_handle_type handle)
{
   return my->preview_builder_transaction(handle);
}

signed_transaction wallet_api::sign_builder_transaction(transaction_handle_type transaction_handle,
                                                        const vector<public_key_type>& explicit_keys,
                                                        bool broadcast)
{
   return my->sign_builder_transaction(transaction_handle, explicit_keys, broadcast);
}

pair<transaction_id_type,signed_transaction> wallet_api::broadcast_transaction(signed_transaction tx)
{
    return my->broadcast_transaction(tx);
}

signed_transaction wallet_api::propose_builder_transaction(
      transaction_handle_type handle,
      string account_name_or_id,
      time_point_sec expiration,
      uint32_t review_period_seconds,
      bool broadcast)
{
   return my->propose_builder_transaction(handle, account_name_or_id, expiration, review_period_seconds, broadcast);
}

void wallet_api::remove_builder_transaction(transaction_handle_type handle)
{
   return my->remove_builder_transaction(handle);
}

account_object wallet_api::get_account(string account_name_or_id) const
{
   return my->get_account(account_name_or_id);
}

extended_asset_object wallet_api::get_asset(string asset_name_or_id) const
{
   auto found_asset = my->find_asset(asset_name_or_id);
   FC_ASSERT( found_asset, "Unable to find asset '${a}'", ("a",asset_name_or_id) );
   return *found_asset;
}

asset_bitasset_data_object wallet_api::get_bitasset_data(string asset_name_or_id) const
{
   auto asset = get_asset(asset_name_or_id);
   FC_ASSERT(asset.is_market_issued() && asset.bitasset_data_id);
   return my->get_object(*asset.bitasset_data_id);
}

account_id_type wallet_api::get_account_id(string account_name_or_id) const
{
   return my->get_account_id(account_name_or_id);
}

asset_id_type wallet_api::get_asset_id(const string& asset_symbol_or_id) const
{
   return my->get_asset_id(asset_symbol_or_id);
}

bool wallet_api::import_key(string account_name_or_id, string wif_key)
{
   FC_ASSERT(!is_locked());
   // backup wallet
   fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
   if (!optional_private_key)
      FC_THROW("Invalid private key");
   string shorthash = detail::address_to_shorthash(optional_private_key->get_public_key());
   copy_wallet_file( "before-import-key-" + shorthash );

   if( my->import_key(account_name_or_id, wif_key) )
   {
      save_wallet_file();
      copy_wallet_file( "after-import-key-" + shorthash );
      return true;
   }
   return false;
}

string wallet_api::normalize_brain_key(string s) const
{
   return detail::normalize_brain_key( s );
}

variant wallet_api::info()
{
   return my->info();
}

variant_object wallet_api::about() const
{
    return my->about();
}

fc::ecc::private_key wallet_api::derive_private_key(const std::string& prefix_string, int sequence_number) const
{
   return detail::derive_private_key( prefix_string, sequence_number );
}

signed_transaction wallet_api::register_account(string name,
                                                public_key_type owner_pubkey,
                                                public_key_type active_pubkey,
                                                string  registrar_account,
                                                string  referrer_account,
                                                uint32_t referrer_percent,
                                                bool broadcast)
{
   return my->register_account( name, owner_pubkey, active_pubkey,
                                registrar_account, referrer_account, referrer_percent, broadcast );
}
signed_transaction wallet_api::create_account_with_brain_key(string brain_key, string account_name,
                                                             string registrar_account, string referrer_account,
                                                             bool broadcast /* = false */)
{
   return my->create_account_with_brain_key(
            brain_key, account_name, registrar_account,
            referrer_account, broadcast
            );
}
signed_transaction wallet_api::issue_asset(string to_account, string amount, string symbol,
                                           string memo, bool broadcast)
{
   return my->issue_asset(to_account, amount, symbol, memo, broadcast);
}

signed_transaction wallet_api::transfer(string from, string to, string amount,
                                        string asset_symbol, string memo, bool broadcast /* = false */)
{
   return my->transfer(from, to, amount, asset_symbol, memo, broadcast);
}
signed_transaction wallet_api::create_asset(string issuer,
                                            string symbol,
                                            uint8_t precision,
                                            asset_options common,
                                            fc::optional<bitasset_options> bitasset_opts,
                                            bool broadcast)

{
   return my->create_asset(issuer, symbol, precision, common, bitasset_opts, broadcast);
}

signed_transaction wallet_api::update_asset(string symbol,
                                            optional<string> new_issuer,
                                            asset_options new_options,
                                            bool broadcast /* = false */)
{
   return my->update_asset(symbol, new_issuer, new_options, broadcast);
}

signed_transaction wallet_api::update_asset_issuer(string symbol,
                                            string new_issuer,
                                            bool broadcast /* = false */)
{
   return my->update_asset_issuer(symbol, new_issuer, broadcast);
}

signed_transaction wallet_api::update_bitasset(string symbol,
                                               bitasset_options new_options,
                                               bool broadcast /* = false */)
{
   return my->update_bitasset(symbol, new_options, broadcast);
}

signed_transaction wallet_api::update_asset_feed_producers(string symbol,
                                                           flat_set<string> new_feed_producers,
                                                           bool broadcast /* = false */)
{
   return my->update_asset_feed_producers(symbol, new_feed_producers, broadcast);
}

signed_transaction wallet_api::publish_asset_feed(string publishing_account,
                                                  string symbol,
                                                  price_feed feed,
                                                  bool broadcast /* = false */)
{
   return my->publish_asset_feed(publishing_account, symbol, feed, broadcast);
}

signed_transaction wallet_api::fund_asset_fee_pool(string from,
                                                   string symbol,
                                                   string amount,
                                                   bool broadcast /* = false */)
{
   return my->fund_asset_fee_pool(from, symbol, amount, broadcast);
}

signed_transaction wallet_api::claim_asset_fee_pool(string symbol,
                                                    string amount,
                                                    bool broadcast /* = false */)
{
   return my->claim_asset_fee_pool(symbol, amount, broadcast);
}

signed_transaction wallet_api::reserve_asset(string from,
                                          string amount,
                                          string symbol,
                                          bool broadcast /* = false */)
{
   return my->reserve_asset(from, amount, symbol, broadcast);
}

signed_transaction wallet_api::global_settle_asset(string symbol,
                                                   price settle_price,
                                                   bool broadcast /* = false */)
{
   return my->global_settle_asset(symbol, settle_price, broadcast);
}

signed_transaction wallet_api::settle_asset(string account_to_settle,
                                            string amount_to_settle,
                                            string symbol,
                                            bool broadcast /* = false */)
{
   return my->settle_asset(account_to_settle, amount_to_settle, symbol, broadcast);
}

signed_transaction wallet_api::whitelist_account(string authorizing_account,
                                                 string account_to_list,
                                                 account_whitelist_operation::account_listing new_listing_status,
                                                 bool broadcast /* = false */)
{
   return my->whitelist_account(authorizing_account, account_to_list, new_listing_status, broadcast);
}

signed_transaction wallet_api::create_committee_member(string owner_account, string url,
                                               bool broadcast /* = false */)
{
   return my->create_committee_member(owner_account, url, broadcast);
}

map<string,witness_id_type> wallet_api::list_witnesses(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_witness_accounts(lowerbound, limit);
}

map<string,committee_member_id_type> wallet_api::list_committee_members(const string& lowerbound, uint32_t limit)
{
   return my->_remote_db->lookup_committee_member_accounts(lowerbound, limit);
}

witness_object wallet_api::get_witness(string owner_account)
{
   return my->get_witness(owner_account);
}

committee_member_object wallet_api::get_committee_member(string owner_account)
{
   return my->get_committee_member(owner_account);
}

signed_transaction wallet_api::create_witness(string owner_account,
                                              string url,
                                              bool broadcast /* = false */)
{
   return my->create_witness(owner_account, url, broadcast);
}

signed_transaction wallet_api::create_worker(
   string owner_account,
   time_point_sec work_begin_date,
   time_point_sec work_end_date,
   share_type daily_pay,
   string name,
   string url,
   variant worker_settings,
   bool broadcast /* = false */)
{
   return my->create_worker( owner_account, work_begin_date, work_end_date,
      daily_pay, name, url, worker_settings, broadcast );
}

signed_transaction wallet_api::update_worker_votes(
   string owner_account,
   worker_vote_delta delta,
   bool broadcast /* = false */)
{
   return my->update_worker_votes( owner_account, delta, broadcast );
}

signed_transaction wallet_api::update_witness(
   string witness_name,
   string url,
   string block_signing_key,
   bool broadcast /* = false */)
{
   return my->update_witness(witness_name, url, block_signing_key, broadcast);
}

vector< vesting_balance_object_with_info > wallet_api::get_vesting_balances( string account_name )
{
   return my->get_vesting_balances( account_name );
}

signed_transaction wallet_api::withdraw_vesting(
   string witness_name,
   string amount,
   string asset_symbol,
   bool broadcast /* = false */)
{
   return my->withdraw_vesting( witness_name, amount, asset_symbol, broadcast );
}

signed_transaction wallet_api::vote_for_committee_member(string voting_account,
                                                 string witness,
                                                 bool approve,
                                                 bool broadcast /* = false */)
{
   return my->vote_for_committee_member(voting_account, witness, approve, broadcast);
}

signed_transaction wallet_api::vote_for_witness(string voting_account,
                                                string witness,
                                                bool approve,
                                                bool broadcast /* = false */)
{
   return my->vote_for_witness(voting_account, witness, approve, broadcast);
}

signed_transaction wallet_api::set_voting_proxy(string account_to_modify,
                                                optional<string> voting_account,
                                                bool broadcast /* = false */)
{
   return my->set_voting_proxy(account_to_modify, voting_account, broadcast);
}

signed_transaction wallet_api::set_desired_witness_and_committee_member_count(string account_to_modify,
                                                                      uint16_t desired_number_of_witnesses,
                                                                      uint16_t desired_number_of_committee_members,
                                                                      bool broadcast /* = false */)
{
   return my->set_desired_witness_and_committee_member_count(account_to_modify, desired_number_of_witnesses,
                                                     desired_number_of_committee_members, broadcast);
}

void wallet_api::set_wallet_filename(string wallet_filename)
{
   my->_wallet_filename = wallet_filename;
}

signed_transaction wallet_api::sign_transaction(signed_transaction tx, bool broadcast /* = false */)
{ try {
   return my->sign_transaction( tx, broadcast);
} FC_CAPTURE_AND_RETHROW( (tx) ) }

signed_transaction wallet_api::sign_transaction2(signed_transaction tx, const vector<public_key_type>& signing_keys,
                                                 bool broadcast /* = false */)
{ try {
   return my->sign_transaction2( tx, signing_keys, broadcast);
} FC_CAPTURE_AND_RETHROW( (tx) ) }

flat_set<public_key_type> wallet_api::get_transaction_signers(const signed_transaction &tx) const
{ try {
   return my->get_transaction_signers(tx);
} FC_CAPTURE_AND_RETHROW( (tx) ) }

vector<flat_set<account_id_type>> wallet_api::get_key_references(const vector<public_key_type> &keys) const
{ try {
   return my->get_key_references(keys);
} FC_CAPTURE_AND_RETHROW( (keys) ) }

operation wallet_api::get_prototype_operation(string operation_name)
{
   return my->get_prototype_operation( operation_name );
}

void wallet_api::dbg_make_uia(string creator, string symbol)
{
   FC_ASSERT(!is_locked());
   my->dbg_make_uia(creator, symbol);
}

void wallet_api::dbg_make_mia(string creator, string symbol)
{
   FC_ASSERT(!is_locked());
   my->dbg_make_mia(creator, symbol);
}

void wallet_api::dbg_push_blocks( std::string src_filename, uint32_t count )
{
   my->dbg_push_blocks( src_filename, count );
}

void wallet_api::dbg_generate_blocks( std::string debug_wif_key, uint32_t count )
{
   my->dbg_generate_blocks( debug_wif_key, count );
}

void wallet_api::dbg_stream_json_objects( const std::string& filename )
{
   my->dbg_stream_json_objects( filename );
}

void wallet_api::dbg_update_object( fc::variant_object update )
{
   my->dbg_update_object( update );
}

void wallet_api::network_add_nodes( const vector<string>& nodes )
{
   my->network_add_nodes( nodes );
}

vector< variant > wallet_api::network_get_connected_peers()
{
   return my->network_get_connected_peers();
}

void wallet_api::flood_network(string prefix, uint32_t number_of_transactions)
{
   FC_ASSERT(!is_locked());
   my->flood_network(prefix, number_of_transactions);
}

signed_transaction wallet_api::propose_parameter_extension_change(
   const string& proposing_account,
   fc::time_point_sec expiration_time,
   const variant_object& changed_extensions,
   bool broadcast /* = false */
   )
{
   return my->propose_parameter_extension_change( proposing_account, expiration_time, changed_extensions, broadcast );
}

signed_transaction wallet_api::propose_parameter_change(
   const string& proposing_account,
   fc::time_point_sec expiration_time,
   const variant_object& changed_values,
   bool broadcast /* = false */
   )
{
   return my->propose_parameter_change( proposing_account, expiration_time, changed_values, broadcast );
}

signed_transaction wallet_api::propose_fee_change(
   const string& proposing_account,
   fc::time_point_sec expiration_time,
   const variant_object& changed_fees,
   bool broadcast /* = false */
   )
{
   return my->propose_fee_change( proposing_account, expiration_time, changed_fees, broadcast );
}

signed_transaction wallet_api::approve_proposal(
   const string& fee_paying_account,
   const string& proposal_id,
   const approval_delta& delta,
   bool broadcast /* = false */
   )
{
   return my->approve_proposal( fee_paying_account, proposal_id, delta, broadcast );
}

global_property_object wallet_api::get_global_properties() const
{
   return my->get_global_properties();
}

dynamic_global_property_object wallet_api::get_dynamic_global_properties() const
{
   return my->get_dynamic_global_properties();
}

signed_transaction wallet_api::add_transaction_signature( signed_transaction tx,
                                                          bool broadcast )
{
   return my->add_transaction_signature( tx, broadcast );
}

signed_transaction wallet_api::create_personal_data(
      const string& subject_account,
      const string& operator_account,
      const string& url,
      const string& hash,
      const string& storage_data,
      bool broadcast )
{ 
   return my->create_personal_data( subject_account, operator_account, url, hash, storage_data, broadcast );
}

signed_transaction wallet_api::remove_personal_data(
      const string& subject_account,
      const string& operator_account,
      const string& hash,
      bool broadcast )
{
   return my->remove_personal_data(subject_account, operator_account, hash, broadcast );
}

std::vector<personal_data_object> wallet_api::get_personal_data(
      const string& subject_account,
      const string& operator_account) const
{
   return my->get_personal_data(subject_account, operator_account);
}

personal_data_object wallet_api::get_last_personal_data(
      const string& subject_account,
      const string& operator_account) const
{
   return my->get_last_personal_data(subject_account, operator_account);
}

signed_transaction wallet_api::create_content_card(
      const string& subject_account,
      const string& hash,
      const string& url,
      const string& type,
      const string& description,
      const string& content_key,
      const string& storage_data,
      bool broadcast ) const
{
   return my->create_content_card(subject_account, hash, url, type, description, content_key, storage_data, broadcast);
}

signed_transaction wallet_api::update_content_card(
      const string& subject_account,
      const string& hash,
      const string& url,
      const string& type,
      const string& description,
      const string& content_key,
      const string& storage_data,
      bool broadcast ) const
{
   return my->update_content_card(subject_account, hash, url, type, description, content_key, storage_data, broadcast);
}

signed_transaction wallet_api::remove_content_card( const string& subject_account,
      uint64_t content_id,
      bool broadcast ) const
{
   return my->remove_content_card(subject_account, content_id, broadcast);
}

signed_transaction wallet_api::create_permission(
      const string& subject_account,
      const string& operator_account,
      const string& permission_type,
      const string& object_id,
      const string& content_key,
      bool broadcast ) const
{
   return my->create_permission(subject_account, operator_account, permission_type, object_id, content_key, broadcast);
}

signed_transaction wallet_api::remove_permission( const string& subject_account,
      uint64_t permission_id,
      bool broadcast ) const
{
   return my->remove_permission(subject_account, permission_id, broadcast);
}

content_card_object wallet_api::get_content_card_by_id( uint64_t content_id ) const
{
   return my->get_content_card_by_id(content_id);
}

std::vector<content_card_object> wallet_api::get_content_cards(
      const string& subject_account,
      uint64_t content_id,
      unsigned limit ) const
{
   return my->get_content_cards(subject_account, content_id, limit);
}

permission_object wallet_api::get_permission_by_id( uint64_t permission_id ) const
{
   return my->get_permission_by_id(permission_id);
}

std::vector<permission_object> wallet_api::get_permissions(
      const string& operator_account,
      uint64_t permission_id,
      unsigned limit ) const
{
   return my->get_permissions(operator_account, permission_id, limit);
}

string wallet_api::help()const
{
   std::vector<std::string> method_names = my->method_documentation.get_method_names();
   std::stringstream ss;
   for (const std::string& method_name : method_names)
   {
      try
      {
         ss << my->method_documentation.get_brief_description(method_name);
      }
      catch (const fc::key_not_found_exception&)
      {
         ss << method_name << " (no help available)\n";
      }
   }
   return ss.str();
}

string wallet_api::gethelp(const string& method)const
{
   fc::api<wallet_api> tmp;
   std::stringstream ss;
   ss << "\n";

   std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
   if (!doxygenHelpString.empty())
      ss << doxygenHelpString << "\n";

   if( method == "import_key" )
   {
      ss << "usage: import_key ACCOUNT_NAME_OR_ID  WIF_PRIVATE_KEY\n\n";
      ss << "example: import_key \"1.3.11\" 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3\n";
      ss << "example: import_key \"usera\" 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3\n";
   }
   else if( method == "transfer" )
   {
      ss << "usage: transfer FROM TO AMOUNT SYMBOL \"memo\" BROADCAST\n\n";
      ss << "example: transfer \"1.3.11\" \"1.3.4\" 1000.03 CORE \"memo\" true\n";
      ss << "example: transfer \"usera\" \"userb\" 1000.123 CORE \"memo\" true\n";
   }
   else if( method == "create_account_with_brain_key" )
   {
      ss << "usage: create_account_with_brain_key BRAIN_KEY ACCOUNT_NAME REGISTRAR REFERRER BROADCAST\n\n";
      ss << "example: create_account_with_brain_key "
         << "\"my really long brain key\" \"newaccount\" \"1.3.11\" \"1.3.11\" true\n";
      ss << "example: create_account_with_brain_key "
         << "\"my really long brain key\" \"newaccount\" \"someaccount\" \"otheraccount\" true\n";
      ss << "\n";
      ss << "This method should be used if you would like the wallet to generate new keys derived from the "
         << "brain key.\n";
      ss << "The BRAIN_KEY will be used as the owner key, and the active key will be derived from the BRAIN_KEY.  "
         << "Use\n";
      ss << "register_account if you already know the keys you know the public keys that you would like to "
         << "register.\n";

   }
   else if( method == "register_account" )
   {
      ss << "usage: register_account ACCOUNT_NAME OWNER_PUBLIC_KEY ACTIVE_PUBLIC_KEY REGISTRAR "
         << "REFERRER REFERRER_PERCENT BROADCAST\n\n";
      ss << "example: register_account \"newaccount\" \"CORE6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\" "
         << "\"CORE6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\" \"1.3.11\" \"1.3.11\" 50 true\n";
      ss << "\n";
      ss << "Use this method to register an account for which you do not know the private keys.";
   }
   else if( method == "create_asset" )
   {
      ss << "usage: ISSUER SYMBOL PRECISION_DIGITS OPTIONS BITASSET_OPTIONS BROADCAST\n\n";
      ss << "PRECISION_DIGITS: the number of digits after the decimal point\n\n";
      ss << "Example value of OPTIONS: \n";
      ss << fc::json::to_pretty_string( graphene::chain::asset_options() );
      ss << "\nExample value of BITASSET_OPTIONS: \n";
      ss << fc::json::to_pretty_string( graphene::chain::bitasset_options() );
      ss << "\nBITASSET_OPTIONS may be null\n";
   }
   else if (doxygenHelpString.empty())
      ss << "No help defined for method " << method << "\n";

   return ss.str();
}

bool wallet_api::load_wallet_file( string wallet_filename )
{
   return my->load_wallet_file( wallet_filename );
}

void wallet_api::quit()
{
   my->quit();
}

void wallet_api::save_wallet_file( string wallet_filename )
{
   my->save_wallet_file( wallet_filename );
}

std::map<string,std::function<string(fc::variant,const fc::variants&)> >
wallet_api::get_result_formatters() const
{
   return my->get_result_formatters();
}

bool wallet_api::is_locked()const
{
   return my->is_locked();
}
bool wallet_api::is_new()const
{
   return my->_wallet.cipher_keys.size() == 0;
}

void wallet_api::encrypt_keys()
{
   my->encrypt_keys();
}

void wallet_api::lock()
{ try {
   FC_ASSERT( !is_locked() );
   encrypt_keys();
   for( auto key : my->_keys )
      key.second = key_to_wif(fc::ecc::private_key());
   my->_keys.clear();
   my->_checksum = fc::sha512();
   my->self.lock_changed(true);
} FC_CAPTURE_AND_RETHROW() }

void wallet_api::unlock(string password)
{ try {
   FC_ASSERT(password.size() > 0);
   auto pw = fc::sha512::hash(password.c_str(), password.size());
   vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
   auto pk = fc::raw::unpack<plain_keys>(decrypted);
   FC_ASSERT(pk.checksum == pw);
   my->_keys = std::move(pk.keys);
   my->_checksum = pk.checksum;
   my->self.lock_changed(false);
} FC_CAPTURE_AND_RETHROW() }

void wallet_api::set_password( string password )
{
   if( !is_new() )
      FC_ASSERT( !is_locked(), "The wallet must be unlocked before the password can be set" );
   my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
   lock();
}

vector< signed_transaction > wallet_api::import_balance(
      string name_or_id,
      const vector<string>& wif_keys,
      bool broadcast )
{
   return my->import_balance( name_or_id, wif_keys, broadcast );
}

vector< signed_transaction > wallet_api::ico_import_balance(
      string account_name_or_id,
      string eth_pub_key,
      string eth_sign,
      bool broadcast )
{
   return my->ico_import_balance( account_name_or_id, eth_pub_key, eth_sign, broadcast );
}

map<public_key_type, string> wallet_api::dump_private_keys()
{
   FC_ASSERT(!is_locked());
   return my->_keys;
}

signed_transaction wallet_api::upgrade_account( string name, bool broadcast )
{
   return my->upgrade_account(name,broadcast);
}

signed_transaction wallet_api::sell_asset(string seller_account,
                                          string amount_to_sell,
                                          string symbol_to_sell,
                                          string min_to_receive,
                                          string symbol_to_receive,
                                          uint32_t expiration,
                                          bool   fill_or_kill,
                                          bool   broadcast)
{
   return my->sell_asset(seller_account, amount_to_sell, symbol_to_sell, min_to_receive,
                         symbol_to_receive, expiration, fill_or_kill, broadcast);
}

signed_transaction wallet_api::cancel_order(object_id_type order_id, bool broadcast)
{
   FC_ASSERT(!is_locked());
   return my->cancel_order(order_id, broadcast);
}

memo_data wallet_api::sign_memo(string from, string to, string memo)
{
   FC_ASSERT(!is_locked());
   return my->sign_memo(from, to, memo);
}

string wallet_api::read_memo(const memo_data& memo)
{
   FC_ASSERT(!is_locked());
   return my->read_memo(memo);
}

signed_message wallet_api::sign_message(string signer, string message)
{
   FC_ASSERT(!is_locked());
   return my->sign_message(signer, message);
}

bool wallet_api::verify_message( string message, string account, int block, const string& time, compact_signature sig )
{
   return my->verify_message( message, account, block, time, sig );
}

/** Verify a message signed with sign_message
 *
 * @param message the signed_message structure containing message, meta data and signature
 * @return true if signature matches
 */
bool wallet_api::verify_signed_message( signed_message message )
{
   return my->verify_signed_message( message );
}

/** Verify a message signed with sign_message, in its encapsulated form.
 *
 * @param message the complete encapsulated message string including separators and line feeds
 * @return true if signature matches
 */
bool wallet_api::verify_encapsulated_message( string message )
{
   return my->verify_encapsulated_message( message );
}

string wallet_api::get_private_key( public_key_type pubkey )const
{
   return key_to_wif( my->get_private_key( pubkey ) );
}

public_key_type  wallet_api::get_public_key( string label )const
{
   try { return fc::variant(label, 1).as<public_key_type>( 1 ); } catch ( ... ){}

   auto key_itr   = my->_wallet.labeled_keys.get<by_label>().find(label);
   if( key_itr != my->_wallet.labeled_keys.get<by_label>().end() )
      return key_itr->key;
   return public_key_type();
}
order_book wallet_api::get_order_book( const string& base, const string& quote, unsigned limit )
{
   return( my->_remote_db->get_order_book( base, quote, limit ) );
}

// custom operations
signed_transaction wallet_api::account_store_map(string account, string catalog, bool remove,
      flat_map<string, optional<string>> key_values, bool broadcast)
{
   return my->account_store_map(account, catalog, remove, key_values, broadcast);
}

vector<account_storage_object> wallet_api::get_account_storage(string account, string catalog)
{ try {
   return my->_custom_operations->get_storage_info(account, catalog);
} FC_CAPTURE_AND_RETHROW( (account)(catalog) ) }

signed_block_with_info::signed_block_with_info( const signed_block& block )
   : signed_block( block )
{
   block_id = id();
   signing_key = signee();
   transaction_ids.reserve( transactions.size() );
   for( const processed_transaction& tx : transactions )
      transaction_ids.push_back( tx.id() );
}

vesting_balance_object_with_info::vesting_balance_object_with_info(
      const vesting_balance_object& vbo,
      fc::time_point_sec now )
   : vesting_balance_object( vbo )
{
   allowed_withdraw = get_allowed_withdraw( now );
   allowed_withdraw_time = now;
}

} } // graphene::wallet

namespace fc {
   void to_variant( const account_multi_index_type& accts, variant& vo, uint32_t max_depth )
   {
      to_variant( std::vector<account_object>(accts.begin(), accts.end()), vo, max_depth );
   }

   void from_variant( const variant& var, account_multi_index_type& vo, uint32_t max_depth )
   {
      const std::vector<account_object>& v = var.as<std::vector<account_object>>( max_depth );
      vo = account_multi_index_type(v.begin(), v.end());
   }
}
