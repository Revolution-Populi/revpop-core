/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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

#include <graphene/app/api_objects.hpp>

#include <graphene/protocol/types.hpp>

#include <graphene/chain/database.hpp>

#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/personal_data_object.hpp>
#include <graphene/chain/personal_data_v2_object.hpp>
#include <graphene/chain/content_card_object.hpp>
#include <graphene/chain/content_card_v2_object.hpp>
#include <graphene/chain/permission_object.hpp>
#include <graphene/chain/content_votfe_object.hpp>
#include <graphene/chain/vote_master_summary_object.hpp>
#include <graphene/chain/commit_reveal_object.hpp>
#include <graphene/chain/commit_reveal_v2_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>

#include <fc/api.hpp>
#include <fc/variant_object.hpp>

#include <fc/network/ip.hpp>

#include <boost/container/flat_set.hpp>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace graphene { namespace app {

using namespace graphene::chain;
using std::string;
using std::vector;
using std::map;

class database_api_impl;

/**
 * @brief The database_api class implements the RPC API for the chain database.
 *
 * This API exposes accessors on the database which query state tracked by a blockchain validating node. This API is
 * read-only; all modifications to the database must be performed via transactions. Transactions are broadcast via
 * the @ref network_broadcast_api.
 */
class database_api
{
   public:
      database_api(graphene::chain::database& db, const application_options* app_options = nullptr );
      ~database_api();

      /////////////
      // Objects //
      /////////////

      /**
       * @brief Get the objects corresponding to the provided IDs
       * @param ids IDs of the objects to retrieve
       * @param subscribe @a true to subscribe to the queried objects; @a false to not subscribe;
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return The objects retrieved, in the order they are mentioned in ids
       * @note operation_history_object (1.11.x) and account_transaction_history_object (2.9.x)
       *       can not be subscribed.
       *
       * If any of the provided IDs does not map to an object, a null variant is returned in its position.
       */
      fc::variants get_objects( const vector<object_id_type>& ids,
                                optional<bool> subscribe = optional<bool>() )const;

      ///////////////////
      // Subscriptions //
      ///////////////////

      /**
       * @brief Register a callback handle which then can be used to subscribe to object database changes
       * @param cb The callback handle to register
       * @param notify_remove_create Whether subscribe to universal object creation and removal events.
       *        If this is set to true, the API server will notify all newly created objects and ID of all
       *        newly removed objects to the client, no matter whether client subscribed to the objects.
       *        By default, API servers don't allow subscribing to universal events, which can be changed
       *        on server startup.
       *
       * Note: auto-subscription is enabled by default and can be disabled with @ref set_auto_subscription API.
       */
      void set_subscribe_callback( std::function<void(const variant&)> cb, bool notify_remove_create );
      /**
       * @brief Set auto-subscription behavior of follow-up API queries
       * @param enable whether follow-up API queries will automatically subscribe to queried objects
       *
       * Impacts behavior of these APIs:
       * - get_accounts
       * - get_assets
       * - get_objects
       * - lookup_accounts
       * - get_full_accounts
       *
       * Note: auto-subscription is enabled by default
       *
       * @see @ref set_subscribe_callback
       */
      void set_auto_subscription( bool enable );
      /**
       * @brief Register a callback handle which will get notified when a transaction is pushed to database
       * @param cb The callback handle to register
       *
       * Note: a transaction can be pushed to database and be popped from database several times while
       *   processing, before and after included in a block. Everytime when a push is done, the client will
       *   be notified.
       */
      void set_pending_transaction_callback( std::function<void(const variant& signed_transaction_object)> cb );
      /**
       * @brief Register a callback handle which will get notified when a block is pushed to database
       * @param cb The callback handle to register
       */
      void set_block_applied_callback( std::function<void(const variant& block_id)> cb );
      /**
       * @brief Stop receiving any notifications
       *
       * This unsubscribes from all subscribed markets and objects.
       */
      void cancel_all_subscriptions();

      /////////////////////////////
      // Blocks and transactions //
      /////////////////////////////

      /**
       * @brief Retrieve a block header
       * @param block_num Height of the block whose header should be returned
       * @return header of the referenced block, or null if no matching block was found
       */
      optional<block_header> get_block_header(uint32_t block_num)const;

      /**
      * @brief Retrieve multiple block header by block numbers
      * @param block_nums vector containing heights of the block whose header should be returned
      * @return array of headers of the referenced blocks, or null if no matching block was found
      */
      map<uint32_t, optional<block_header>> get_block_header_batch(const vector<uint32_t> block_nums)const;

      /**
       * @brief Retrieve a full, signed block
       * @param block_num Height of the block to be returned
       * @return the referenced block, or null if no matching block was found
       */
      optional<signed_block> get_block(uint32_t block_num)const;

      /**
       * @brief used to fetch an individual transaction.
       * @param block_num height of the block to fetch
       * @param trx_in_block the index (sequence number) of the transaction in the block, starts from 0
       * @return the transaction at the given position
       */
      processed_transaction get_transaction( uint32_t block_num, uint32_t trx_in_block )const;

      /**
       * If the transaction has not expired, this method will return the transaction for the given ID or
       * it will return NULL if it is not known.  Just because it is not known does not mean it wasn't
       * included in the blockchain.
       *
       * @param txid hash of the transaction
       * @return the corresponding transaction if found, or null if not found
       */
      optional<signed_transaction> get_recent_transaction_by_id( const transaction_id_type& txid )const;

      /////////////
      // Globals //
      /////////////

      /**
       * @brief Retrieve the @ref graphene::chain::chain_property_object associated with the chain
       */
      chain_property_object get_chain_properties()const;

      /**
       * @brief Retrieve the current @ref graphene::chain::global_property_object
       */
      global_property_object get_global_properties()const;

      /**
       * @brief Retrieve compile-time constants
       */
      fc::variant_object get_config()const;

      /**
       * @brief Get the chain ID
       */
      chain_id_type get_chain_id()const;

      /**
       * @brief Retrieve the current @ref graphene::chain::dynamic_global_property_object
       */
      dynamic_global_property_object get_dynamic_global_properties()const;

      /**
       * @brief Retrieve the current @ref graphene::chain::witness_schedule_object
       */
      witness_schedule_object get_witness_schedule()const;

      //////////
      // Keys //
      //////////

      /**
       * @brief Get all accounts that refer to the specified public keys in their owner authority, active authorities
       *        or memo key
       * @param keys a list of public keys to query
       * @return ID of all accounts that refer to the specified keys
       */
      vector<flat_set<account_id_type>> get_key_references( vector<public_key_type> keys )const;

      /**
       * Determine whether a textual representation of a public key
       * (in Base-58 format) is *currently* linked
       * to any *registered* (i.e. non-stealth) account on the blockchain
       * @param public_key Public key
       * @return Whether a public key is known
       */
      bool is_public_key_registered(string public_key) const;

      //////////////
      // Accounts //
      //////////////

      /**
       * @brief Get account object from a name or ID
       * @param name_or_id name or ID of the account
       * @return Account ID
       *
       */
      account_id_type get_account_id_from_string(const std::string& name_or_id) const;

      /**
       * @brief Get a list of accounts by names or IDs
       * @param account_names_or_ids names or IDs of the accounts to retrieve
       * @param subscribe @a true to subscribe to the queried account objects; @a false to not subscribe;
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return The accounts corresponding to the provided names or IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<account_object>> get_accounts( const vector<std::string>& account_names_or_ids,
                                                     optional<bool> subscribe = optional<bool>() )const;

      /**
       * @brief Fetch all objects relevant to the specified accounts and optionally subscribe to updates
       * @param names_or_ids Each item must be the name or ID of an account to retrieve
       * @param subscribe @a true to subscribe to the queried full account objects; @a false to not subscribe;
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return Map of string from @p names_or_ids to the corresponding account
       *
       * This function fetches all relevant objects for the given accounts, and subscribes to updates to the given
       * accounts. If any of the strings in @p names_or_ids cannot be tied to an account, that input will be
       * ignored. All other accounts will be retrieved and subscribed.
       *
       */
      std::map<string,full_account> get_full_accounts( const vector<string>& names_or_ids,
                                                       optional<bool> subscribe = optional<bool>() );

      /**
       * @brief Get info of an account by name
       * @param name Name of the account to retrieve
       * @return The account holding the provided name
       */
      optional<account_object> get_account_by_name( string name )const;

      /**
       * @brief Get all accounts that refer to the specified account in their owner or active authorities
       * @param account_name_or_id Account name or ID to query
       * @return all accounts that refer to the specified account in their owner or active authorities
       */
      vector<account_id_type> get_account_references( const std::string account_name_or_id )const;

      /**
       * @brief Get a list of accounts by name
       * @param account_names Names of the accounts to retrieve
       * @return The accounts holding the provided names
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe.
       */
      vector<optional<account_object>> lookup_account_names(const vector<string>& account_names)const;

      /**
       * @brief Get names and IDs for registered accounts
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return -- must not exceed 1000
       * @param subscribe @a true to subscribe to the queried account objects; @a false to not subscribe;
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return Map of account names to corresponding IDs
       *
       * @note In addition to the common auto-subscription rules,
       *       this API will subscribe to the returned account only if @p limit is 1.
       */
      map<string,account_id_type> lookup_accounts( const string& lower_bound_name,
                                                   uint32_t limit,
                                                   optional<bool> subscribe = optional<bool>() )const;

      //////////////
      // Balances //
      //////////////

      /**
       * @brief Get an account's balances in various assets
       * @param account_name_or_id name or ID of the account to get balances for
       * @param assets IDs of the assets to get balances of; if empty, get all assets account has a balance in
       * @return Balances of the account
       */
      vector<asset> get_account_balances( const std::string& account_name_or_id,
                                          const flat_set<asset_id_type>& assets )const;

      /// Semantically equivalent to @ref get_account_balances.
      vector<asset> get_named_account_balances(const std::string& name, const flat_set<asset_id_type>& assets)const;

      /**
       * @brief Return all unclaimed balance objects for a list of addresses
       * @param addrs a list of addresses
       * @return all unclaimed balance objects for the addresses
       */
      vector<balance_object> get_balance_objects( const vector<address>& addrs )const;

      /**
       * @brief Calculate how much assets in the given balance objects are able to be claimed at current head
       *        block time
       * @param objs a list of balance object IDs
       * @return a list indicating how much asset in each balance object is available to be claimed
       */
      vector<asset> get_vested_balances( const vector<balance_id_type>& objs )const;

      /**
       * @brief Return all vesting balance objects owned by an account
       * @param account_name_or_id name or ID of an account
       * @return all vesting balance objects owned by the account
       */
      vector<vesting_balance_object> get_vesting_balances( const std::string account_name_or_id )const;

      /**
       * @brief Get the total number of accounts registered with the blockchain
       */
      uint64_t get_account_count()const;

      ////////////
      // Assets //
      ////////////

      /**
       * @brief Get asset ID from an asset symbol or ID
       * @param symbol_or_id symbol name or ID of the asset
       * @return asset ID
       */
      asset_id_type get_asset_id_from_string(const std::string& symbol_or_id) const;

      /**
       * @brief Get a list of assets by symbol names or IDs
       * @param asset_symbols_or_ids symbol names or IDs of the assets to retrieve
       * @param subscribe @a true to subscribe to the queried asset objects; @a false to not subscribe;
       *                  @a null to subscribe or not subscribe according to current auto-subscription setting
       *                  (see @ref set_auto_subscription)
       * @return The assets corresponding to the provided symbol names or IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<extended_asset_object>> get_assets( const vector<std::string>& asset_symbols_or_ids,
                                                          optional<bool> subscribe = optional<bool>() )const;

      /**
       * @brief Get assets alphabetically by symbol name
       * @param lower_bound_symbol Lower bound of symbol names to retrieve
       * @param limit Maximum number of assets to fetch (must not exceed 101)
       * @return The assets found
       */
      vector<extended_asset_object> list_assets(const string& lower_bound_symbol, uint32_t limit)const;

      /**
       * @brief Get a list of assets by symbol names or IDs
       * @param symbols_or_ids symbol names or IDs of the assets to retrieve
       * @return The assets corresponding to the provided symbols or IDs
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe
       */
      vector<optional<extended_asset_object>> lookup_asset_symbols(const vector<string>& symbols_or_ids)const;

      /**
       * @brief Get assets count
       * @return The assets count
       */
      uint64_t get_asset_count()const;

      /**
       * @brief Get assets issued (owned) by a given account
       * @param issuer_name_or_id Account name or ID to get objects from
       * @param start Asset objects(1.3.X) before this ID will be skipped in results. Pagination purposes.
       * @param limit Maximum number of orders to retrieve
       * @return The assets issued (owned) by the account
       */
      vector<extended_asset_object> get_assets_by_issuer(const std::string& issuer_name_or_id,
                                                         asset_id_type start, uint32_t limit)const;

      ///////////////
      // Witnesses //
      ///////////////

      /**
       * @brief Get a list of witnesses by ID
       * @param witness_ids IDs of the witnesses to retrieve
       * @return The witnesses corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe
       */
      vector<optional<witness_object>> get_witnesses(const vector<witness_id_type>& witness_ids)const;

      /**
       * @brief Get the witness owned by a given account
       * @param account_name_or_id The name or ID of the account whose witness should be retrieved
       * @return The witness object, or null if the account does not have a witness
       */
      fc::optional<witness_object> get_witness_by_account(const std::string account_name_or_id)const;

      /**
       * @brief Get names and IDs for registered witnesses
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return -- must not exceed 1000
       * @return Map of witness names to corresponding IDs
       */
      map<string, witness_id_type> lookup_witness_accounts(const string& lower_bound_name, uint32_t limit)const;

      /**
       * @brief Get the total number of witnesses registered with the blockchain
       */
      uint64_t get_witness_count()const;

      ///////////////////////
      // Committee members //
      ///////////////////////

      /**
       * @brief Get a list of committee_members by ID
       * @param committee_member_ids IDs of the committee_members to retrieve
       * @return The committee_members corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects, but doesn't subscribe
       */
      vector<optional<committee_member_object>> get_committee_members(
            const vector<committee_member_id_type>& committee_member_ids)const;

      /**
       * @brief Get the committee_member owned by a given account
       * @param account_name_or_id The name or ID of the account whose committee_member should be retrieved
       * @return The committee_member object, or null if the account does not have a committee_member
       */
      fc::optional<committee_member_object> get_committee_member_by_account( const string account_name_or_id )const;

      /**
       * @brief Get names and IDs for registered committee_members
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return -- must not exceed 1000
       * @return Map of committee_member names to corresponding IDs
       */
      map<string, committee_member_id_type> lookup_committee_member_accounts( const string& lower_bound_name,
                                                                              uint32_t limit )const;

      /**
       * @brief Get the total number of committee registered with the blockchain
      */
      uint64_t get_committee_count()const;


      ///////////////////////
      // Worker proposals  //
      ///////////////////////

      /**
       * @brief Get workers
       * @param is_expired null for all workers, true for expired workers only, false for non-expired workers only
       * @return A list of worker objects
       *
      */
      vector<worker_object> get_all_workers( const optional<bool> is_expired = optional<bool>() )const;

      /**
       * @brief Get the workers owned by a given account
       * @param account_name_or_id The name or ID of the account whose worker should be retrieved
       * @return A list of worker objects owned by the account
       */
      vector<worker_object> get_workers_by_account(const std::string account_name_or_id)const;

      /**
       * @brief Get the total number of workers registered with the blockchain
      */
      uint64_t get_worker_count()const;



      ///////////
      // Votes //
      ///////////

      /**
       * @brief Given a set of votes, return the objects they are voting for
       * @param votes a list of vote IDs
       * @return the referenced objects
       *
       * This will be a mixture of committee_member_objects, witness_objects, and worker_objects
       *
       * The results will be in the same order as the votes.  Null will be returned for
       * any vote IDs that are not found.
       */
      vector<variant> lookup_vote_ids( const vector<vote_id_type>& votes )const;

      ////////////////////////////
      // Authority / validation //
      ////////////////////////////

      /**
       * @brief Get a hexdump of the serialized binary form of a transaction
       * @param trx a transaction to get hexdump from
       * @return the hexdump of the transaction
       */
      std::string get_transaction_hex(const signed_transaction& trx)const;

      /**
       * @brief Get a hexdump of the serialized binary form of a signatures-stripped transaction
       * @param trx a transaction to get hexdump from
       * @return the hexdump of the transaction without the signatures
       */
      std::string get_transaction_hex_without_sig( const signed_transaction &trx ) const;

      /**
       *  This API will take a partially signed transaction and a set of public keys that the owner
       *  has the ability to sign for and return the minimal subset of public keys that should add
       *  signatures to the transaction.
       *
       *  @param trx the transaction to be signed
       *  @param available_keys a set of public keys
       *  @return a subset of @p available_keys that could sign for the given transaction
       */
      set<public_key_type> get_required_signatures( const signed_transaction& trx,
                                                    const flat_set<public_key_type>& available_keys )const;

      /**
       *  This method will return the set of all public keys that could possibly sign for a given transaction.
       *  This call can be used by wallets to filter their set of public keys to just the relevant subset prior
       *  to calling @ref get_required_signatures to get the minimum subset.
       *
       *  @param trx the transaction to be signed
       *  @return a set of public keys that could possibly sign for the given transaction
       */
      set<public_key_type> get_potential_signatures( const signed_transaction& trx )const;

      /**
       *  This method will return the set of all addresses that could possibly sign for a given transaction.
       *
       *  @param trx the transaction to be signed
       *  @return a set of addresses that could possibly sign for the given transaction
       */
      set<address> get_potential_address_signatures( const signed_transaction& trx )const;

      /**
       * Check whether a transaction has all of the required signatures
       * @param trx a transaction to be verified
       * @return true if the @p trx has all of the required signatures, otherwise throws an exception
       */
      bool           verify_authority( const signed_transaction& trx )const;

      /**
       * @brief Verify that the public keys have enough authority to approve an operation for this account
       * @param account_name_or_id name or ID of an account to check
       * @param signers the public keys
       * @return true if the passed in keys have enough authority to approve an operation for this account
       */
      bool verify_account_authority( const string& account_name_or_id,
                                     const flat_set<public_key_type>& signers )const;

      /**
       * @brief Validates a transaction against the current state without broadcasting it on the network
       * @param trx a transaction to be validated
       * @return a processed_transaction object if the transaction passes the validation,
                 otherwise an exception will be thrown
       */
      processed_transaction validate_transaction( const signed_transaction& trx )const;

      /**
       * @brief For each operation calculate the required fee in the specified asset type
       * @param ops a list of operations to be query for required fees
       * @param asset_symbol_or_id symbol name or ID of an asset that to be used to pay the fees
       * @return a list of objects which indicates required fees of each operation
       */
      vector< fc::variant > get_required_fees( const vector<operation>& ops,
                                               const std::string& asset_symbol_or_id )const;

      ///////////////////////////
      // Proposed transactions //
      ///////////////////////////

      /**
       * @brief return a set of proposed transactions (aka proposals) that the specified account
       *        can add approval to or remove approval from
       * @param account_name_or_id The name or ID of an account
       * @return a set of proposed transactions that the specified account can act on
       */
      vector<proposal_object> get_proposed_transactions( const std::string account_name_or_id )const;

      //////////////////////
      // Blinded balances //
      //////////////////////

      /**
       * @brief return the set of blinded balance objects by commitment ID
       * @param commitments a set of commitments to query for
       * @return the set of blinded balance objects by commitment ID
       */
      vector<blinded_balance_object> get_blinded_balances( const flat_set<commitment_type>& commitments )const;

      /////////////////
      // Withdrawals //
      /////////////////

      /**
       *  @brief Get non expired withdraw permission objects for a giver(ex:recurring customer)
       *  @param account_name_or_id Account name or ID to get objects from
       *  @param start Withdraw permission objects(1.12.X) before this ID will be skipped in results.
       *               Pagination purposes.
       *  @param limit Maximum number of objects to retrieve
       *  @return Withdraw permission objects for the account
       */
      vector<withdraw_permission_object> get_withdraw_permissions_by_giver( const std::string account_name_or_id,
                                                                            withdraw_permission_id_type start,
                                                                            uint32_t limit )const;

      /**
       *  @brief Get non expired withdraw permission objects for a recipient(ex:service provider)
       *  @param account_name_or_id Account name or ID to get objects from
       *  @param start Withdraw permission objects(1.12.X) before this ID will be skipped in results.
       *               Pagination purposes.
       *  @param limit Maximum number of objects to retrieve
       *  @return Withdraw permission objects for the account
       */
      vector<withdraw_permission_object> get_withdraw_permissions_by_recipient( const std::string account_name_or_id,
                                                                                withdraw_permission_id_type start,
                                                                                uint32_t limit )const;

      ////////////
      // RevPop //
      ////////////

      /**
       * @brief Get personal data
       * @param owner_account The owner of personal data.
       * @param permission_account An account who is permitted to use personal data.
       * @return The personal data object list
      */
      vector<personal_data_object> get_personal_data( const account_id_type subject_account,
                                                      const account_id_type operator_account ) const;
      /**
       * @brief Get personal data with maximum id
       * @param owner_account The owner of personal data.
       * @param permission_account An account who is permitted to use personal data.
       * @return The personal data object
       */
      fc::optional<personal_data_object> get_last_personal_data( const account_id_type subject_account,
                                                                 const account_id_type operator_account ) const;

      /**
       * @brief Get personal data v2
       * @param owner_account The owner of personal data.
       * @param permission_account An account who is permitted to use personal data.
       * @return The personal data object list
      */
      vector<personal_data_v2_object> get_personal_data_v2( const account_id_type subject_account,
                                                      const account_id_type operator_account ) const;
      /**
       * @brief Get personal data v2 with maximum id
       * @param owner_account The owner of personal data.
       * @param permission_account An account who is permitted to use personal data.
       * @return The personal data object
       */
      fc::optional<personal_data_v2_object> get_last_personal_data_v2( const account_id_type subject_account,
                                                                 const account_id_type operator_account ) const;

      /**
       * @brief Get content card by id
       * @param content_id The id of content card
       * @return The content card object
       */
      fc::optional<content_card_object> get_content_card_by_id( const content_card_id_type content_id ) const;

      /**
       * @brief Get list of content cards
       * @param subject_account The owner account of the content
       * @param content_id Lower bound of content id to start getting results
       * @param limit Maximum number of content card objects to fetch
       * @return The content card object list
       */
      vector<content_card_object> get_content_cards( const account_id_type subject_account,
                                                     const content_card_id_type content_id, uint32_t limit ) const;

      /**
       * @brief Get content card by id
       * @param content_id The id of content card
       * @return The content card object
       */
      fc::optional<content_card_v2_object> get_content_card_v2_by_id( const content_card_v2_id_type content_id ) const;

      /**
       * @brief Get list of content cards
       * @param subject_account The owner account of the content
       * @param content_id Lower bound of content id to start getting results
       * @param limit Maximum number of content card objects to fetch
       * @return The content card object list
       */
      vector<content_card_v2_object> get_content_cards_v2( const account_id_type subject_account,
                                                     const content_card_v2_id_type content_id, uint32_t limit ) const;

      /**
       * @brief Get permission object by id
       * @param permission_id The id of permission object
       * @return The permission object
       */
      fc::optional<permission_object> get_permission_by_id( const permission_id_type permission_id ) const;

      /**
       * @brief Get list of permission objects
       * @param operator_account The owner account of the permissions
       * @param permission_id Lower bound of permission id to start getting results
       * @param limit Maximum number of permission objects to fetch
       * @return The list of permission objects
       */
      vector<permission_object> get_permissions( const account_id_type operator_account,
                                                 const permission_id_type permission_id, uint32_t limit ) const;
      /**
       * @brief Get content vote object by id
       * @param content_id The content vote is
       * @return The content vote object
       */
      fc::optional<content_vote_object> get_content_vote( const string& content_id ) const;

      /**
       * @brief Get list of content vote objects
       * @param subject_account The owner account of content votes
       * @param start Lower bound of content vote id to start getting results
       * @param limit Maximum number of permission objects to fetch
       * @return The list of content vote objects
       */
      vector<content_vote_object> get_content_votes( const account_id_type subject_account,
                                                     const string& start, uint32_t limit ) const;

      /**
       * @brief Get vote statistic by master accounts
       * @param start Lower bound of vote master summary id to start getting results
       * @param limit Maximum number of objects to fetch
       * @return The list of vote master summary objects
       */
      vector<vote_master_summary_object> get_vote_stat( const vote_master_summary_id_type start, uint32_t limit ) const;

      /**
       * @brief Get commit-reveal object by account
       * @param account An account to getting commit-reveal object
       * @return The commit-reveal object
       */
      fc::optional<commit_reveal_object> get_account_commit_reveal( const account_id_type account ) const;
      fc::optional<commit_reveal_v2_object> get_account_commit_reveal_v2( const account_id_type account ) const;

      /**
       * @brief Allow get all commit-reveal objects fro database
       * @param start Lower bound of commit-reveal id to start getting results
       * @param limit Maximum number of objects to fetch
       * @return The list of commit-reveal objects
       */
      vector<commit_reveal_object> get_commit_reveals( const commit_reveal_id_type start, uint32_t limit ) const;
      vector<commit_reveal_v2_object> get_commit_reveals_v2( const commit_reveal_v2_id_type start, uint32_t limit ) const;

      /**
       * @brief Get commit reveal seed
       * @param accounts List of accounts which reveals will be used to calculate seed
       * @return The seed number
       */
      uint64_t get_commit_reveal_seed(const vector<account_id_type>& accounts) const;
      uint64_t get_commit_reveal_seed_v2(const vector<account_id_type>& accounts) const;

      /**
       * @brief Get list of account id which reveals are filled
       * @param accounts The list of accounts for filtering
       * @return The list of account ids
       */
      vector<account_id_type> filter_commit_reveal_participant(const vector<account_id_type>& accounts) const;
      vector<account_id_type> filter_commit_reveal_participant_v2(const vector<account_id_type>& accounts) const;

private:
      std::shared_ptr< database_api_impl > my;
};

} }

extern template class fc::api<graphene::app::database_api>;

FC_API(graphene::app::database_api,
   // Objects
   (get_objects)

   // Subscriptions
   (set_subscribe_callback)
   (set_auto_subscription)
   (set_pending_transaction_callback)
   (set_block_applied_callback)
   (cancel_all_subscriptions)

   // Blocks and transactions
   (get_block_header)
   (get_block_header_batch)
   (get_block)
   (get_transaction)
   (get_recent_transaction_by_id)

   // Globals
   (get_chain_properties)
   (get_global_properties)
   (get_config)
   (get_chain_id)
   (get_dynamic_global_properties)
   (get_witness_schedule)

   // Keys
   (get_key_references)
   (is_public_key_registered)

   // Accounts
   (get_account_id_from_string)
   (get_accounts)
   (get_full_accounts)
   (get_account_by_name)
   (get_account_references)
   (lookup_account_names)
   (lookup_accounts)
   (get_account_count)

   // Balances
   (get_account_balances)
   (get_named_account_balances)
   (get_balance_objects)
   (get_vested_balances)
   (get_vesting_balances)

   // Assets
   (get_assets)
   (list_assets)
   (lookup_asset_symbols)
   (get_asset_count)
   (get_assets_by_issuer)
   (get_asset_id_from_string)

   // Witnesses
   (get_witnesses)
   (get_witness_by_account)
   (lookup_witness_accounts)
   (get_witness_count)

   // Committee members
   (get_committee_members)
   (get_committee_member_by_account)
   (lookup_committee_member_accounts)
   (get_committee_count)

   // workers
   (get_all_workers)
   (get_workers_by_account)
   (get_worker_count)

   // Votes
   (lookup_vote_ids)

   // Authority / validation
   (get_transaction_hex)
   (get_transaction_hex_without_sig)
   (get_required_signatures)
   (get_potential_signatures)
   (get_potential_address_signatures)
   (verify_authority)
   (verify_account_authority)
   (validate_transaction)
   (get_required_fees)

   // Proposed transactions
   (get_proposed_transactions)

   // Blinded balances
   (get_blinded_balances)

   // Withdrawals
   (get_withdraw_permissions_by_giver)
   (get_withdraw_permissions_by_recipient)

   // PevPop
   (get_personal_data)
   (get_last_personal_data)
   (get_content_card_by_id)
   (get_content_cards)
   (get_permission_by_id)
   (get_permissions)
   (get_content_vote)
   (get_content_votes)
   (get_vote_stat)
   (get_account_commit_reveal)
   (get_commit_reveals)
   (get_commit_reveal_seed)
   (filter_commit_reveal_participant)
   (get_account_commit_reveal_v2)
   (get_commit_reveals_v2)
   (get_commit_reveal_seed_v2)
   (filter_commit_reveal_participant_v2)
   (get_content_card_v2_by_id)
   (get_content_cards_v2)
   (get_personal_data_v2)
   (get_last_personal_data_v2)
)
