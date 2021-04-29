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

#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/withdraw_permission_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

#define UIA_TEST_SYMBOL "UIATEST"

BOOST_FIXTURE_TEST_SUITE( operation_tests, database_fixture )

BOOST_AUTO_TEST_CASE( feed_limit_logic_test )
{
   try {
      asset usd(1000,asset_id_type(1));
      asset core(1000,asset_id_type(0));
      price_feed feed;
      feed.settlement_price = usd / core;

      // require 3x min collateral
      auto swanp = usd / core;
      auto callp = ~price::call_price( usd, core, 1750 );
      // 1:1 collateral
//      wdump((callp.to_real())(callp));
//      wdump((swanp.to_real())(swanp));
      FC_ASSERT( callp.to_real() > swanp.to_real() );

      /*
      wdump((feed.settlement_price.to_real()));
      wdump((feed.maintenance_price().to_real()));
      wdump((feed.max_short_squeeze_price().to_real()));

      BOOST_CHECK( usd * feed.settlement_price < usd * feed.maintenance_price() );
      BOOST_CHECK( usd * feed.maintenance_price() < usd * feed.max_short_squeeze_price() );
      */

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( asset_settle_cancel_operation_test_after_hf588 )
{
   set_expiration( db, trx );

   BOOST_TEST_MESSAGE( "Creating a proposal containing a asset_settle_cancel_operation" );
   {
      proposal_create_operation pcop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time());
      pcop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      pcop.expiration_time = db.head_block_time() + *pcop.review_period_seconds + 10;
      asset_settle_cancel_operation ascop;
      ascop.amount.amount = 1;
      pcop.proposed_ops.emplace_back(ascop);
      trx.operations.push_back(pcop);

      BOOST_CHECK_EXCEPTION(PUSH_TX(db, trx), fc::assert_exception,
            [](fc::assert_exception const &e) -> bool {
               std::cout << e.to_string() << std::endl;
               if (e.to_string().find("Virtual operation") != std::string::npos)
                  return true;

               return false;
            });
   }

   BOOST_TEST_MESSAGE( "Creating a recursive proposal containing asset_settle_cancel_operation" );
   {
      proposal_create_operation pcop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time());

      pcop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      pcop.expiration_time = db.head_block_time() + *pcop.review_period_seconds + 10;
      proposal_create_operation inner_pcop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time());

      inner_pcop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      inner_pcop.expiration_time = db.head_block_time() + *inner_pcop.review_period_seconds + 10;

      asset_settle_cancel_operation ascop;
      ascop.amount.amount = 1;
      inner_pcop.proposed_ops.emplace_back(ascop);
      pcop.proposed_ops.emplace_back(inner_pcop);

      trx.operations.push_back(pcop);

      BOOST_CHECK_EXCEPTION(PUSH_TX(db, trx), fc::assert_exception,
            [](fc::assert_exception const &e) -> bool {
               std::cout << e.to_string() << std::endl;
               if (e.to_string().find("Virtual operation") != std::string::npos)
                  return true;

               return false;
            });
   }
}

/// Test case for bsip77:
/// * the "initial_collateral_ratio" parameter can only be set after the BSIP77 hard fork
/// * the parameter should be within a range
// TODO removed the hard fork part after the hard fork, keep the valid range part
BOOST_AUTO_TEST_CASE( bsip77_hardfork_time_and_param_valid_range_test )
{
   try {

      // Proceeds to a recent hard fork
      generate_block();
      set_expiration( db, trx );

      ACTORS((sam));

      // Before bsip77 hard fork, unable to create a bitasset with ICR
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 0 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 1 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 1000 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 1001 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 1750 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 32000 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBIT", sam_id, 100, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 32001 ), fc::exception );

      // Can create a bitasset without ICR
      const auto& bitusd = create_bitasset( "USDBIT", sam.id, 100, charge_market_fee, 2, {},
                                            GRAPHENE_MAX_SHARE_SUPPLY );
      asset_id_type usd_id = bitusd.id;

      // helper function for setting ICR for an asset
      auto set_icr_for_asset = [&](asset_id_type aid, optional<uint16_t> icr) {
         const asset_object& ao = aid(db);
         const asset_bitasset_data_object& abo = ao.bitasset_data(db);
         asset_update_bitasset_operation uop;
         uop.issuer = ao.issuer;
         uop.asset_to_update = aid;
         uop.new_options = abo.options;
         uop.new_options.extensions.value.initial_collateral_ratio = icr;
         trx.operations.clear();
         trx.operations.push_back( uop );
         trx.validate();
         set_expiration( db, trx );
         PUSH_TX(db, trx, ~0);
      };

      // Before bsip77 hard fork, unable to update a bitasset with ICR
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 0 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 1 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 1000 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 1001 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 1750 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 32000 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 32001 ), fc::exception );

      // helper function for creating a proposal which contains an asset_create_operation with ICR
      auto propose_create_bitasset = [&]( string name, optional<uint16_t> icr ) {
         asset_create_operation acop = make_bitasset( name, sam_id, 100, charge_market_fee, 2, {},
                                                      GRAPHENE_MAX_SHARE_SUPPLY, icr );
         proposal_create_operation cop;
         cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
         cop.expiration_time = db.head_block_time() + 100;
         cop.proposed_ops.emplace_back( acop );
         trx.operations.clear();
         trx.operations.push_back( cop );
         trx.validate();
         set_expiration( db, trx );
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.operations.clear();
      };

      // Before bsip77 hard fork, unable to create a proposal with an asset_create_operation with ICR
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 0 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 1 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 1000 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 1001 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 1750 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 32000 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITA", 32001 ), fc::exception );

      // helper function for creating a proposal which contains an asset_update_bitasset_operation with ICR
      auto propose_set_icr_for_asset = [&](asset_id_type aid, optional<uint16_t> icr) {
         const asset_object& ao = aid(db);
         const asset_bitasset_data_object& abo = ao.bitasset_data(db);
         asset_update_bitasset_operation uop;
         uop.issuer = ao.issuer;
         uop.asset_to_update = aid;
         uop.new_options = abo.options;
         uop.new_options.extensions.value.initial_collateral_ratio = icr;

         proposal_create_operation cop;
         cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
         cop.expiration_time = db.head_block_time() + 100;
         cop.proposed_ops.emplace_back( uop );
         trx.operations.clear();
         trx.operations.push_back( cop );
         trx.validate();
         set_expiration( db, trx );
         PUSH_TX(db, trx, ~0);
         trx.operations.clear();
      };

      // Before bsip77 hard fork, unable to create a proposal with an asset_update_bitasset_op with ICR
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 0 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 1 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 1000 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 1001 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 1750 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 32000 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 32001 ), fc::exception );

      // Pass the hard fork time
      generate_blocks( HARDFORK_BSIP_77_TIME );
      set_expiration( db, trx );

      // Unable to create a bitasset with an invalid ICR
      BOOST_CHECK_THROW( create_bitasset( "USDBITB", sam_id, 0, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 0 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBITB", sam_id, 1, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 0 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBITB", sam_id, 1000, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 0 ), fc::exception );
      BOOST_CHECK_THROW( create_bitasset( "USDBITB", sam_id, 32001, charge_market_fee, 2, {},
                                          GRAPHENE_MAX_SHARE_SUPPLY, 0 ), fc::exception );
      // Able to create a bitasset with a valid ICR
      asset_id_type usdc_id = create_bitasset( "USDBITC", sam.id, 100, charge_market_fee, 2, {},
                                               GRAPHENE_MAX_SHARE_SUPPLY, 1001 ).id;
      asset_id_type usdd_id = create_bitasset( "USDBITD", sam.id, 100, charge_market_fee, 2, {},
                                               GRAPHENE_MAX_SHARE_SUPPLY, 1750 ).id;
      asset_id_type usde_id = create_bitasset( "USDBITE", sam.id, 100, charge_market_fee, 2, {},
                                               GRAPHENE_MAX_SHARE_SUPPLY, 32000 ).id;
      // Able to create a bitasset without ICR
      asset_id_type usdf_id = create_bitasset( "USDBITF", sam.id, 100, charge_market_fee, 2, {},
                                               GRAPHENE_MAX_SHARE_SUPPLY, {} ).id;

      BOOST_CHECK( usdc_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio == 1001 );
      BOOST_CHECK( usdd_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio == 1750 );
      BOOST_CHECK( usde_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio == 32000 );
      BOOST_CHECK( !usdf_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio.valid() );

      // Unable to update a bitasset with an invalid ICR
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 0 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 1 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 1000 ), fc::exception );
      BOOST_CHECK_THROW( set_icr_for_asset( usd_id, 32001 ), fc::exception );
      // Able to update a bitasset with a valid ICR
      set_icr_for_asset( usd_id, 1001 );
      BOOST_CHECK( usd_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio == 1001 );
      set_icr_for_asset( usd_id, 1750 );
      BOOST_CHECK( usd_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio == 1750 );
      set_icr_for_asset( usd_id, 32000 );
      BOOST_CHECK( usd_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio == 32000 );
      // Able to update a bitasset, unset its ICR
      set_icr_for_asset( usd_id, {} );
      BOOST_CHECK( !usd_id(db).bitasset_data(db).options.extensions.value.initial_collateral_ratio.valid() );

      // Unable to create a proposal with an asset_create_operation with an invalid ICR
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITG", 0 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITG", 1 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITG", 1000 ), fc::exception );
      BOOST_CHECK_THROW( propose_create_bitasset( "USDBITG", 32001 ), fc::exception );
      // able to create a proposal with a valid ICR or no ICR
      propose_create_bitasset( "USDBITG", 1001 );
      propose_create_bitasset( "USDBITG", 1750 );
      propose_create_bitasset( "USDBITG", 32000 );
      propose_create_bitasset( "USDBITG", {} );

      // Unable to create a proposal with an asset_update_bitasset_op with an invalid ICR
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 0 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 1 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 1000 ), fc::exception );
      BOOST_CHECK_THROW( propose_set_icr_for_asset( usd_id, 32001 ), fc::exception );
      // Able to create a proposal with a valid ICR or no ICR
      propose_set_icr_for_asset( usd_id, 1001 );
      propose_set_icr_for_asset( usd_id, 1750 );
      propose_set_icr_for_asset( usd_id, 32000 );
      propose_set_icr_for_asset( usd_id, {} );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_account_test )
{
   try {
      set_expiration( db, trx );
      trx.operations.push_back(make_account());
      account_create_operation op = trx.operations.back().get<account_create_operation>();

      REQUIRE_THROW_WITH_VALUE(op, registrar, account_id_type(9999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-1));
      REQUIRE_THROW_WITH_VALUE(op, name, "!");
      REQUIRE_THROW_WITH_VALUE(op, name, "Sam");
      REQUIRE_THROW_WITH_VALUE(op, name, "saM");
      REQUIRE_THROW_WITH_VALUE(op, name, "sAm");
      REQUIRE_THROW_WITH_VALUE(op, name, "6j");
      REQUIRE_THROW_WITH_VALUE(op, name, "j-");
      REQUIRE_THROW_WITH_VALUE(op, name, "-j");
      REQUIRE_THROW_WITH_VALUE(op, name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
      REQUIRE_THROW_WITH_VALUE(op, name, "aaaa.");
      REQUIRE_THROW_WITH_VALUE(op, name, ".aaaa");
      REQUIRE_THROW_WITH_VALUE(op, options.voting_account, account_id_type(999999999));

      // Not allow voting for non-exist entities.
      auto save_num_committee = op.options.num_committee;
      auto save_num_witness = op.options.num_witness;
      op.options.num_committee = 1;
      op.options.num_witness = 0;
      REQUIRE_THROW_WITH_VALUE(op, options.votes, boost::assign::list_of<vote_id_type>(vote_id_type("0:1")).convert_to_container<flat_set<vote_id_type>>());
      op.options.num_witness = 1;
      op.options.num_committee = 0;
      REQUIRE_THROW_WITH_VALUE(op, options.votes, boost::assign::list_of<vote_id_type>(vote_id_type("1:19")).convert_to_container<flat_set<vote_id_type>>());
      op.options.num_witness = 0;
      REQUIRE_THROW_WITH_VALUE(op, options.votes, boost::assign::list_of<vote_id_type>(vote_id_type("2:19")).convert_to_container<flat_set<vote_id_type>>());
      REQUIRE_THROW_WITH_VALUE(op, options.votes, boost::assign::list_of<vote_id_type>(vote_id_type("3:99")).convert_to_container<flat_set<vote_id_type>>());
      GRAPHENE_REQUIRE_THROW( vote_id_type("2:a"), fc::exception );
      GRAPHENE_REQUIRE_THROW( vote_id_type(""), fc::exception );
      op.options.num_committee = save_num_committee;
      op.options.num_witness = save_num_witness;

      auto auth_bak = op.owner;
      op.owner.add_authority(account_id_type(9999999999), 10);
      trx.operations.back() = op;
      op.owner = auth_bak;
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);
      op.owner = auth_bak;

      trx.operations.back() = op;
      sign( trx,  init_account_priv_key );
      trx.validate();
      PUSH_TX( db, trx, ~0 );

      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");
      BOOST_CHECK(nathan_account.id.space() == protocol_ids);
      BOOST_CHECK(nathan_account.id.type() == account_object_type);
      BOOST_CHECK(nathan_account.name == "nathan");

      BOOST_REQUIRE(nathan_account.owner.num_auths() == 1);
      BOOST_CHECK(nathan_account.owner.key_auths.at(committee_key) == 123);
      BOOST_REQUIRE(nathan_account.active.num_auths() == 1);
      BOOST_CHECK(nathan_account.active.key_auths.at(committee_key) == 321);
      BOOST_CHECK(nathan_account.options.voting_account == GRAPHENE_PROXY_TO_SELF_ACCOUNT);
      BOOST_CHECK(nathan_account.options.memo_key == committee_key);

      const account_statistics_object& statistics = nathan_account.statistics(db);
      BOOST_CHECK(statistics.id.space() == implementation_ids);
      BOOST_CHECK(statistics.id.type() == impl_account_statistics_object_type);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_account )
{
   try {
      const account_object& nathan = create_account("nathan", init_account_pub_key);
      const fc::ecc::private_key nathan_new_key = fc::ecc::private_key::generate();
      const public_key_type key_id = nathan_new_key.get_public_key();
      const auto& active_committee_members = db.get_global_properties().active_committee_members;

      transfer(account_id_type()(db), nathan, asset(1000000000));

      trx.operations.clear();
      account_update_operation op;
      op.account = nathan.id;
      op.owner = authority(2, key_id, 1, init_account_pub_key, 1);
      op.active = authority(2, key_id, 1, init_account_pub_key, 1);
      op.new_options = nathan.options;
      op.new_options->votes = flat_set<vote_id_type>({active_committee_members[0](db).vote_id, active_committee_members[5](db).vote_id});
      op.new_options->num_committee = 2;
      trx.operations.push_back(op);
      BOOST_TEST_MESSAGE( "Updating account" );
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK(nathan.options.memo_key == init_account_pub_key);
      BOOST_CHECK(nathan.active.weight_threshold == 2);
      BOOST_CHECK(nathan.active.num_auths() == 2);
      BOOST_CHECK(nathan.active.key_auths.at(key_id) == 1);
      BOOST_CHECK(nathan.active.key_auths.at(init_account_pub_key) == 1);
      BOOST_CHECK(nathan.owner.weight_threshold == 2);
      BOOST_CHECK(nathan.owner.num_auths() == 2);
      BOOST_CHECK(nathan.owner.key_auths.at(key_id) == 1);
      BOOST_CHECK(nathan.owner.key_auths.at(init_account_pub_key) == 1);
      BOOST_CHECK(nathan.options.votes.size() == 2);

      enable_fees();
      {
         account_upgrade_operation op;
         op.account_to_upgrade = nathan.id;
         op.upgrade_to_lifetime_member = true;
         op.fee = db.get_global_properties().parameters.get_current_fees().calculate_fee(op);
         trx.operations = {op};
         PUSH_TX( db, trx, ~0 );
      }

      BOOST_CHECK( nathan.is_lifetime_member() );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer_core_asset )
{
   try {
      INVOKE(create_account_test);

      account_id_type committee_account;
      asset committee_balance = db.get_balance(account_id_type(), asset_id_type());

      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");
      transfer_operation top;
      top.from = committee_account;
      top.to = nathan_account.id;
      top.amount = asset( 10000);
      trx.operations.push_back(top);
      for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);

      asset fee = trx.operations.front().get<transfer_operation>().fee;
      trx.validate();
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(account_id_type()(db), asset_id_type()(db)),
                        (committee_balance.amount - 10000 - fee.amount).value);
      committee_balance = db.get_balance(account_id_type(), asset_id_type());

      BOOST_CHECK_EQUAL(get_balance(nathan_account, asset_id_type()(db)), 10000);

      trx = signed_transaction();
      top.from = nathan_account.id;
      top.to = committee_account;
      top.amount = asset(2000);
      trx.operations.push_back(top);

      for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);

      fee = trx.operations.front().get<transfer_operation>().fee;
      set_expiration( db, trx );
      trx.validate();
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(nathan_account, asset_id_type()(db)), 8000 - fee.amount.value);
      BOOST_CHECK_EQUAL(get_balance(account_id_type()(db), asset_id_type()(db)), committee_balance.amount.value + 2000);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_committee_member )
{
   try {
      committee_member_create_operation op;
      op.committee_member_account = account_id_type();
      op.fee = asset();
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, committee_member_account, account_id_type(99999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-600));
      trx.operations.back() = op;

      committee_member_id_type committee_member_id = db.get_index_type<committee_member_index>().get_next_id();
      PUSH_TX( db, trx, ~0 );
      const committee_member_object& d = committee_member_id(db);

      BOOST_CHECK(d.committee_member_account == account_id_type());
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_mia )
{
   try {
      const asset_object& bitusd = create_bitasset( "USDBIT" );
      BOOST_CHECK(bitusd.symbol == "USDBIT");
      BOOST_CHECK(bitusd.bitasset_data(db).options.short_backing_asset == asset_id_type());
      BOOST_CHECK(bitusd.dynamic_asset_data_id(db).current_supply == 0);
      GRAPHENE_REQUIRE_THROW( create_bitasset("USDBIT"), fc::exception);
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_mia )
{
   try {
      INVOKE(create_mia);
      generate_block();
      const asset_object& bit_usd = get_asset("USDBIT");

      asset_update_operation op;
      op.issuer = bit_usd.issuer;
      op.asset_to_update = bit_usd.id;
      op.new_options = bit_usd.options;
      trx.operations.emplace_back(op);

      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );
      std::swap(op.new_options.flags, op.new_options.issuer_permissions);
      //op.new_issuer = account_id_type();
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );
      {
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = bit_usd.id;
         upd_op.issuer = bit_usd.issuer;
         upd_op.new_issuer = account_id_type();
         trx.operations.back() = upd_op;
         PUSH_TX( db, trx, ~0 );
      }

      {
         asset_publish_feed_operation pop;
         pop.asset_id = bit_usd.get_id();
         pop.publisher = get_account("init0").get_id();
         price_feed feed;
         feed.settlement_price = feed.core_exchange_rate = price(bit_usd.amount(5), bit_usd.amount(5));
         REQUIRE_THROW_WITH_VALUE(pop, feed, feed);
         feed.settlement_price = feed.core_exchange_rate = ~price(bit_usd.amount(5), asset(5));
         REQUIRE_THROW_WITH_VALUE(pop, feed, feed);
         feed.settlement_price = feed.core_exchange_rate = price(bit_usd.amount(5), asset(5));
         pop.feed = feed;
         REQUIRE_THROW_WITH_VALUE(pop, feed.maintenance_collateral_ratio, 0);
         trx.operations.back() = pop;
         PUSH_TX( db, trx, ~0 );
      }

      trx.operations.clear();
      auto nathan = create_account("nathan");
      op.issuer = account_id_type();
      //op.new_issuer = nathan.id;
      trx.operations.emplace_back(op);
      PUSH_TX( db, trx, ~0 );
      {
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = bit_usd.id;
         upd_op.issuer = account_id_type();
         upd_op.new_issuer = nathan.id;
         trx.operations.back() = upd_op;
         PUSH_TX( db, trx, ~0 );
      }
      BOOST_CHECK(bit_usd.issuer == nathan.id);

      op.issuer = nathan.id;
      //op.new_issuer = account_id_type();
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );
      {
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = bit_usd.id;
         upd_op.issuer = nathan.id;
         upd_op.new_issuer = account_id_type();
         trx.operations.back() = upd_op;
         PUSH_TX( db, trx, ~0 );
      }
      BOOST_CHECK(bit_usd.issuer == account_id_type());
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}


BOOST_AUTO_TEST_CASE( create_uia )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      creator.symbol = UIA_TEST_SYMBOL;
      creator.common_options.max_supply = 100000000;
      creator.precision = 2;
      creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.common_options.issuer_permissions = DEFAULT_UIA_ASSET_ISSUER_PERMISSION;
      creator.common_options.flags = charge_market_fee;
      creator.common_options.core_exchange_rate = price(asset(2),asset(1,asset_id_type(1)));
      trx.operations.push_back(std::move(creator));
      PUSH_TX( db, trx, ~0 );

      const asset_object& test_asset = test_asset_id(db);
      BOOST_CHECK(test_asset.symbol == UIA_TEST_SYMBOL);
      BOOST_CHECK(asset(1, test_asset_id) * test_asset.options.core_exchange_rate == asset(2));
      BOOST_CHECK((test_asset.options.flags & white_list) == 0);
      BOOST_CHECK(test_asset.options.max_supply == 100000000);
      BOOST_CHECK(!test_asset.bitasset_data_id.valid());
      BOOST_CHECK(test_asset.options.market_fee_percent == GRAPHENE_MAX_MARKET_FEE_PERCENT/100);
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);

      const asset_dynamic_data_object& test_asset_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
      BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_asset_dynamic_data.fee_pool == 0);

      auto op = trx.operations.back().get<asset_create_operation>();
      op.symbol = "TESTFAIL";
      REQUIRE_THROW_WITH_VALUE(op, issuer, account_id_type(99999999));
      REQUIRE_THROW_WITH_VALUE(op, common_options.max_supply, -1);
      REQUIRE_THROW_WITH_VALUE(op, common_options.max_supply, 0);
      REQUIRE_THROW_WITH_VALUE(op, symbol, "A");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "qqq");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "11");
      REQUIRE_THROW_WITH_VALUE(op, symbol, ".AAA");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "AAA.");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "AB CD");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      REQUIRE_THROW_WITH_VALUE(op, common_options.core_exchange_rate, price(asset(-100), asset(1)));
      REQUIRE_THROW_WITH_VALUE(op, common_options.core_exchange_rate, price(asset(100),asset(-1)));
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_uia )
{
   using namespace graphene;
   try {
      INVOKE(create_uia);
      const auto& test = get_asset(UIA_TEST_SYMBOL);
      const auto& nathan = create_account("nathan");

      asset_update_operation op;
      op.issuer = test.issuer;
      op.asset_to_update = test.id;
      op.new_options = test.options;

      trx.operations.push_back(op);

      //Cannot change issuer to same as before
      BOOST_TEST_MESSAGE( "Make sure changing issuer to same as before is forbidden" );
      REQUIRE_THROW_WITH_VALUE(op, new_issuer, test.issuer);

      //Cannot convert to an MIA
      BOOST_TEST_MESSAGE( "Make sure we can't convert UIA to MIA" );
      REQUIRE_THROW_WITH_VALUE(op, new_options.issuer_permissions, ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK);
      REQUIRE_THROW_WITH_VALUE(op, new_options.core_exchange_rate, price(asset(5), asset(5)));

      BOOST_TEST_MESSAGE( "Test updating core_exchange_rate" );
      op.new_options.core_exchange_rate = price(asset(3), test.amount(5));
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );
      REQUIRE_THROW_WITH_VALUE(op, new_options.core_exchange_rate, price());
      op.new_options.core_exchange_rate = test.options.core_exchange_rate;
      //op.new_issuer = nathan.id;
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );
      {
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = test.id;
         upd_op.issuer = test.issuer;
         upd_op.new_issuer = nathan.id;
         trx.operations.back() = upd_op;
         PUSH_TX( db, trx, ~0 );
      }

      BOOST_TEST_MESSAGE( "Test setting flags" );
      op.issuer = nathan.id;
      op.new_options.flags = transfer_restricted | white_list;
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );

      BOOST_TEST_MESSAGE( "Disable white_list permission" );
      op.new_options.issuer_permissions = test.options.issuer_permissions & ~white_list;
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );

      BOOST_TEST_MESSAGE( "Can't toggle white_list" );
      REQUIRE_THROW_WITH_VALUE(op, new_options.flags, test.options.flags & ~white_list);

      BOOST_TEST_MESSAGE( "Can toggle transfer_restricted" );
      for( int i=0; i<2; i++ )
      {
         op.new_options.flags = test.options.flags ^ transfer_restricted;
         trx.operations.back() = op;
         PUSH_TX( db, trx, ~0 );
      }

      asset_issue_operation issue_op;
      issue_op.issuer = op.issuer;
      issue_op.asset_to_issue =  asset(5000000,op.asset_to_update);
      issue_op.issue_to_account = nathan.get_id();
      trx.operations.push_back(issue_op);
      PUSH_TX(db, trx, ~0);
      
      BOOST_TEST_MESSAGE( "Make sure white_list can't be re-enabled (after tokens issued)" );
      op.new_options.issuer_permissions = test.options.issuer_permissions;
      op.new_options.flags = test.options.flags;
      BOOST_CHECK(!(test.options.issuer_permissions & white_list));
      REQUIRE_THROW_WITH_VALUE(op, new_options.issuer_permissions, DEFAULT_UIA_ASSET_ISSUER_PERMISSION);

      BOOST_TEST_MESSAGE( "We can change issuer to account_id_type(), but can't do it again" );
      {
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = test.id;
         upd_op.issuer = nathan.id;
         upd_op.new_issuer = account_id_type();
         trx.operations.back() = upd_op;
         PUSH_TX( db, trx, ~0 );
      };
      op.issuer = account_id_type();
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);
      //op.new_issuer.reset();
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_uia_issuer )
{
   using namespace graphene;
   using namespace graphene::chain;
   using namespace graphene::chain::test;
   try {

      // Lambda for creating accounts with 2 different keys
      auto create_account_2_keys = [&]( const string name,
           fc::ecc::private_key active,
           fc::ecc::private_key owner ) {

         trx.operations.push_back(make_account());
         account_create_operation op = trx.operations.back().get<account_create_operation>();
         op.name = name;
         op.active = authority(1, public_key_type(active.get_public_key()), 1);
         op.owner = authority(1, public_key_type(owner.get_public_key()), 1);
         signed_transaction trx;
         trx.operations.push_back(op);
         db.current_fee_schedule().set_fee( trx.operations.back() );
         set_expiration( db, trx );
         PUSH_TX( db, trx, ~0 );

         return get_account(name);
      };

      auto update_asset_issuer = [&](const asset_object& current,
                                     const account_object& new_issuer) {
         asset_update_operation op;
         op.issuer =  current.issuer;
         op.asset_to_update = current.id;
         op.new_options = current.options;
         op.new_issuer = new_issuer.id;
         signed_transaction tx;
         tx.operations.push_back( op );
         db.current_fee_schedule().set_fee( tx.operations.back() );
         set_expiration( db, tx );
         PUSH_TX( db, tx, ~0 );
      };

      // Lambda for updating the issuer on chain using a particular key
      auto update_issuer = [&](const asset_id_type asset_id,
                               const account_object& issuer,
                               const account_object& new_issuer,
                               const fc::ecc::private_key& key)
      {
         asset_update_issuer_operation op;
         op.issuer = issuer.id;
         op.new_issuer = new_issuer.id;
         op.asset_to_update = asset_id;
         signed_transaction tx;
         tx.operations.push_back( op );
         db.current_fee_schedule().set_fee( tx.operations.back() );
         set_expiration( db, tx );
         sign(tx, key);
         PUSH_TX( db, tx, database::skip_transaction_dupe_check );
      };

      auto update_issuer_proposal = [&](const asset_id_type asset_id,
                                        const account_object& issuer,
                                        const account_object& new_issuer,
                                        const fc::ecc::private_key& key)
      {
          asset_update_issuer_operation op;
          op.issuer = issuer.id;
          op.new_issuer = new_issuer.id;
          op.asset_to_update = asset_id;

          const auto& curfees = db.get_global_properties().parameters.get_current_fees();
          const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
          proposal_create_operation prop;
          prop.fee_paying_account = issuer.id;
          prop.proposed_ops.emplace_back( op );
          prop.expiration_time =  db.head_block_time() + fc::days(1);
          prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

          signed_transaction tx;
          tx.operations.push_back( prop );
          db.current_fee_schedule().set_fee( tx.operations.back() );
          set_expiration( db, tx );
          sign( tx, key );
          PUSH_TX( db, tx );

      };

      // Create alice account
      fc::ecc::private_key alice_owner  = fc::ecc::private_key::regenerate(fc::digest("key1"));
      fc::ecc::private_key alice_active = fc::ecc::private_key::regenerate(fc::digest("key2"));
      fc::ecc::private_key bob_owner    = fc::ecc::private_key::regenerate(fc::digest("key3"));
      fc::ecc::private_key bob_active   = fc::ecc::private_key::regenerate(fc::digest("key4"));

      // Create accounts
      const auto& alice = create_account_2_keys("alice", alice_active, alice_owner);
      const auto& bob = create_account_2_keys("bob", bob_active, bob_owner);
      const account_id_type alice_id = alice.id;
      const account_id_type bob_id = bob.id;

      // Create asset
      const auto& test = create_user_issued_asset("UPDATEISSUER", alice_id(db), 0);
      const asset_id_type test_id = test.id;

      update_issuer_proposal( test_id, alice_id(db), bob_id(db), alice_owner);

      BOOST_TEST_MESSAGE( "Can't change issuer if not my asset" );
      GRAPHENE_REQUIRE_THROW( update_issuer( test_id, bob_id(db), alice_id(db), bob_active ), fc::exception );
      GRAPHENE_REQUIRE_THROW( update_issuer( test_id, bob_id(db), alice_id(db), bob_owner ), fc::exception );

      BOOST_TEST_MESSAGE( "Can't change issuer with alice's active key" );
      GRAPHENE_REQUIRE_THROW( update_issuer( test_id, alice_id(db), bob_id(db), alice_active ), fc::exception );

      BOOST_TEST_MESSAGE( "Old method with asset_update needs to fail" );
      GRAPHENE_REQUIRE_THROW( update_asset_issuer( test_id(db), bob_id(db)  ), fc::exception );

      BOOST_TEST_MESSAGE( "Updating issuer to bob" );
      update_issuer( test_id, alice_id(db), bob_id(db), alice_owner );

      BOOST_CHECK(test_id(db).issuer == bob_id);

   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_uia )
{
   try {
      INVOKE(create_uia);
      INVOKE(create_account_test);

      const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(UIA_TEST_SYMBOL);
      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");

      asset_issue_operation op;
      op.issuer = test_asset.issuer;
      op.asset_to_issue =  test_asset.amount(5000000);
      op.issue_to_account = nathan_account.id;
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, asset_to_issue, asset(200));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-1));
      REQUIRE_THROW_WITH_VALUE(op, issue_to_account, account_id_type(999999999));

      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );

      const asset_dynamic_data_object& test_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset), 5000000);
      BOOST_CHECK(test_dynamic_data.current_supply == 5000000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_dynamic_data.fee_pool == 0);

      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset), 10000000);
      BOOST_CHECK(test_dynamic_data.current_supply == 10000000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_dynamic_data.fee_pool == 0);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer_uia )
{
   try {
      INVOKE(issue_uia);

      const asset_object& uia = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(UIA_TEST_SYMBOL);
      const account_object& nathan = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");
      const account_object& committee = account_id_type()(db);

      BOOST_CHECK_EQUAL(get_balance(nathan, uia), 10000000);
      transfer_operation top;
      top.from = nathan.id;
      top.to = committee.id;
      top.amount = uia.amount(5000);
      trx.operations.push_back(top);
      BOOST_TEST_MESSAGE( "Transfering 5000 TEST from nathan to committee" );
      PUSH_TX( db, trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(nathan, uia), 10000000 - 5000);
      BOOST_CHECK_EQUAL(get_balance(committee, uia), 5000);

      PUSH_TX( db, trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(nathan, uia), 10000000 - 10000);
      BOOST_CHECK_EQUAL(get_balance(committee, uia), 10000);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( uia_fees )
{
   try {
      INVOKE( issue_uia );

      enable_fees();

      const asset_object& test_asset = get_asset(UIA_TEST_SYMBOL);
      const asset_dynamic_data_object& asset_dynamic = test_asset.dynamic_asset_data_id(db);
      const account_object& nathan_account = get_account("nathan");
      const account_object& committee_account = account_id_type()(db);
      const share_type prec = asset::scaled_precision( asset_id_type()(db).precision );

      fund_fee_pool(committee_account, test_asset, 1000*prec);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000*prec);

      transfer_operation op;
      op.fee = test_asset.amount(0);
      op.from = nathan_account.id;
      op.to   = committee_account.id;
      op.amount = test_asset.amount(100);
      op.fee = db.current_fee_schedule().calculate_fee( op, test_asset.options.core_exchange_rate );
      BOOST_CHECK(op.fee.asset_id == test_asset.id);
      asset old_balance = db.get_balance(nathan_account.get_id(), test_asset.get_id());
      asset fee = op.fee;
      BOOST_CHECK(fee.amount > 0);
      asset core_fee = fee*test_asset.options.core_exchange_rate;
      trx.operations.push_back(std::move(op));
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset),
                        (old_balance - fee - test_asset.amount(100)).amount.value);
      BOOST_CHECK_EQUAL(get_balance(committee_account, test_asset), 100);
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000*prec - core_fee.amount);

      //Do it again, for good measure.
      PUSH_TX( db, trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset),
                        (old_balance - fee - fee - test_asset.amount(200)).amount.value);
      BOOST_CHECK_EQUAL(get_balance(committee_account, test_asset), 200);
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount + fee.amount);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000*prec - core_fee.amount - core_fee.amount);

      op = std::move(trx.operations.back().get<transfer_operation>());
      trx.operations.clear();
      op.amount = asset(20);

      BOOST_CHECK_EQUAL(get_balance(nathan_account, asset_id_type()(db)), 0);
      transfer(committee_account, nathan_account, asset(20));
      BOOST_CHECK_EQUAL(get_balance(nathan_account, asset_id_type()(db)), 20);

      trx.operations.emplace_back(std::move(op));
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(nathan_account, asset_id_type()(db)), 0);
      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset),
                        (old_balance - fee - fee - fee - test_asset.amount(200)).amount.value);
      BOOST_CHECK_EQUAL(get_balance(committee_account, test_asset), 200);
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount.value * 3);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000*prec - core_fee.amount.value * 3);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( witness_feeds )
{
   using namespace graphene::chain;
   try {
      INVOKE( create_mia );
      {
         auto& current = get_asset( "USDBIT" );
         asset_update_operation uop;
         uop.issuer =  current.issuer;
         uop.asset_to_update = current.id;
         uop.new_options = current.options;
         //uop.new_issuer = account_id_type();
         trx.operations.push_back(uop);
         PUSH_TX( db, trx, ~0 );
         trx.clear();
      }
      {
         auto& current = get_asset( "USDBIT" );
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = current.id;
         upd_op.issuer = current.issuer;
         upd_op.new_issuer = account_id_type();
         trx.operations.push_back(upd_op);
         PUSH_TX( db, trx, ~0 );
         trx.clear();
      }
      generate_block();
      const asset_object& bit_usd = get_asset("USDBIT");
      auto& global_props = db.get_global_properties();
      vector<account_id_type> active_witnesses;
      for( const witness_id_type& wit_id : global_props.active_witnesses )
         active_witnesses.push_back( wit_id(db).witness_account );
      BOOST_REQUIRE_EQUAL(active_witnesses.size(), INITIAL_WITNESS_COUNT);

      asset_publish_feed_operation op;
      op.publisher = active_witnesses[0];
      op.asset_id = bit_usd.get_id();
      op.feed.settlement_price = op.feed.core_exchange_rate = ~price(asset(GRAPHENE_BLOCKCHAIN_PRECISION),bit_usd.amount(30));
      // Accept defaults for required collateral
      trx.operations.emplace_back(op);
      PUSH_TX( db, trx, ~0 );

      const asset_bitasset_data_object& bitasset = bit_usd.bitasset_data(db);
      BOOST_CHECK(bitasset.current_feed.settlement_price.to_real() == 30.0 / GRAPHENE_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bitasset.current_feed.maintenance_collateral_ratio == GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.publisher = active_witnesses[1];
      op.feed.settlement_price = op.feed.core_exchange_rate = ~price(asset(GRAPHENE_BLOCKCHAIN_PRECISION),bit_usd.amount(25));
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(bitasset.current_feed.settlement_price.to_real(), 30.0 / GRAPHENE_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bitasset.current_feed.maintenance_collateral_ratio == GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.publisher = active_witnesses[2];
      op.feed.settlement_price = op.feed.core_exchange_rate = ~price(asset(GRAPHENE_BLOCKCHAIN_PRECISION),bit_usd.amount(40));
      // But this witness is an idiot.
      op.feed.maintenance_collateral_ratio = 1001;
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(bitasset.current_feed.settlement_price.to_real(), 30.0 / GRAPHENE_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bitasset.current_feed.maintenance_collateral_ratio == GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);
   } catch (const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( witness_pay_test )
{ try {

   const share_type prec = asset::scaled_precision( asset_id_type()(db).precision );

   // there is an immediate maintenance interval in the first block
   //   which will initialize last_budget_time
   generate_block();

   // Make an account and upgrade it to prime, so that witnesses get some pay
   create_account("nathan", init_account_pub_key);
   transfer(account_id_type()(db), get_account("nathan"), asset(20000*prec));
   transfer(account_id_type()(db), get_account("init3"), asset(20*prec));
   generate_block();

   auto last_witness_vbo_balance = [&]() -> share_type
   {
      const witness_object& wit = db.fetch_block_by_number(db.head_block_num())->witness(db);
      if( !wit.pay_vb.valid() )
         return 0;
      return (*wit.pay_vb)(db).balance.amount;
   };

   const auto block_interval = db.get_global_properties().parameters.block_interval;
   const asset_object* core = &asset_id_type()(db);
   const account_object* nathan = &get_account("nathan");
   enable_fees();
   BOOST_CHECK_GT(db.current_fee_schedule().get<account_upgrade_operation>().membership_lifetime_fee, 0u);
   // Based on the size of the reserve fund later in the test, the witness budget will be set to this value
   const uint64_t ref_budget =
      ((uint64_t( db.current_fee_schedule().get<account_upgrade_operation>().membership_lifetime_fee )
         * GRAPHENE_CORE_ASSET_CYCLE_RATE * 30
         * block_interval
       ) + ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS)-1)
      ) >> GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS
      ;
   // change this if ref_budget changes
   BOOST_CHECK_EQUAL( ref_budget, 594u );
   const uint64_t witness_ppb = ref_budget * 10 / 23 + 1;
   // change this if ref_budget changes
   BOOST_CHECK_EQUAL( witness_ppb, 259u );
   // following two inequalities need to hold for maximal code coverage
   BOOST_CHECK_LT( witness_ppb * 2, ref_budget );
   BOOST_CHECK_GT( witness_ppb * 3, ref_budget );

   db.modify( db.get_global_properties(), [&]( global_property_object& _gpo )
   {
      _gpo.parameters.witness_pay_per_block = witness_ppb;
   } );

   BOOST_CHECK_EQUAL(core->dynamic_asset_data_id(db).accumulated_fees.value, 0);
   BOOST_TEST_MESSAGE( "Upgrading account" );
   account_upgrade_operation uop;
   uop.account_to_upgrade = nathan->get_id();
   uop.upgrade_to_lifetime_member = true;
   set_expiration( db, trx );
   trx.operations.push_back(uop);
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   sign( trx, init_account_priv_key );
   PUSH_TX( db, trx );
   auto pay_fee_time = db.head_block_time().sec_since_epoch();
   trx.clear();
   BOOST_CHECK( get_balance(*nathan, *core) == 20000*prec - account_upgrade_operation::fee_parameters_type().membership_lifetime_fee );;

   generate_block();
   nathan = &get_account("nathan");
   core = &asset_id_type()(db);
   BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );

   auto schedule_maint = [&]()
   {
      // now we do maintenance
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.next_maintenance_time = db.head_block_time() + 1;
      } );
   };
   BOOST_TEST_MESSAGE( "Generating some blocks" );

   // generate some blocks
   while( db.head_block_time().sec_since_epoch() - pay_fee_time < 24 * block_interval )
   {
      generate_block();
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );
   }
   BOOST_CHECK_EQUAL( db.head_block_time().sec_since_epoch() - pay_fee_time, 24u * block_interval );

   schedule_maint();
   // The 80% lifetime referral fee went to the committee account, which burned it. Check that it's here.
   BOOST_CHECK( core->reserved(db).value == 8000*prec );
   generate_block();
   BOOST_CHECK_EQUAL( core->reserved(db).value, 999999406 );
   BOOST_CHECK_EQUAL( db.get_dynamic_global_properties().witness_budget.value, (int64_t)ref_budget );
   // first witness paid from old budget (so no pay)
   BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );
   // second witness finally gets paid!
   generate_block();
   BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, (int64_t)witness_ppb );
   BOOST_CHECK_EQUAL( db.get_dynamic_global_properties().witness_budget.value, (int64_t)(ref_budget - witness_ppb) );

   generate_block();
   BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, (int64_t)witness_ppb );
   BOOST_CHECK_EQUAL( db.get_dynamic_global_properties().witness_budget.value, (int64_t)(ref_budget - 2 * witness_ppb) );

   generate_block();
   BOOST_CHECK_LT( last_witness_vbo_balance().value, (int64_t)witness_ppb );
   BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, (int64_t)(ref_budget - 2 * witness_ppb) );
   BOOST_CHECK_EQUAL( db.get_dynamic_global_properties().witness_budget.value, 0 );

   generate_block();
   BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );
   BOOST_CHECK_EQUAL( db.get_dynamic_global_properties().witness_budget.value, 0 );
   BOOST_CHECK_EQUAL(core->reserved(db).value, 999999406 );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( vesting_balance_create_test )
{ try {
   INVOKE( create_uia );

   const asset_object& core = asset_id_type()(db);
   const asset_object& test_asset = get_asset(UIA_TEST_SYMBOL);

   vesting_balance_create_operation op;
   op.fee = core.amount( 0 );
   op.creator = account_id_type();
   op.owner = account_id_type();
   op.amount = test_asset.amount( 100 );
   //op.vesting_seconds = 60*60*24;
   op.policy = cdd_vesting_policy_initializer{ 60*60*24 };

   // Fee must be non-negative
   REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(1) );
   REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(0) );
   REQUIRE_OP_VALIDATION_FAILURE( op, fee, core.amount(-1) );

   // Amount must be positive
   REQUIRE_OP_VALIDATION_SUCCESS( op, amount, core.amount(1) );
   REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount(0) );
   REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount(-1) );

   // Setup world state we will need to test actual evaluation
   const account_object& alice_account = create_account("alice");
   const account_object& bob_account = create_account("bob");

   transfer(committee_account(db), alice_account, core.amount(100000));

   op.creator = alice_account.get_id();
   op.owner = alice_account.get_id();

   account_id_type nobody = account_id_type(1234);

   trx.operations.push_back(op);
   // Invalid account_id's
   REQUIRE_THROW_WITH_VALUE( op, creator, nobody );
   REQUIRE_THROW_WITH_VALUE( op,   owner, nobody );

   // Insufficient funds
   REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(999999999) );
   // Alice can fund a bond to herself or to Bob
   op.amount = core.amount( 1000 );
   REQUIRE_OP_EVALUATION_SUCCESS( op, owner, alice_account.get_id() );
   REQUIRE_OP_EVALUATION_SUCCESS( op, owner,   bob_account.get_id() );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( vesting_balance_withdraw_test )
{ try {
   INVOKE( create_uia );
   // required for head block time
   generate_block();

   const asset_object& core = asset_id_type()(db);
   const asset_object& test_asset = get_asset( UIA_TEST_SYMBOL );

   vesting_balance_withdraw_operation op;
   op.fee = core.amount( 0 );
   op.vesting_balance = vesting_balance_id_type();
   op.owner = account_id_type();
   op.amount = test_asset.amount( 100 );

   // Fee must be non-negative
   REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(  1 )  );
   REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(  0 )  );
   REQUIRE_OP_VALIDATION_FAILURE( op, fee, core.amount( -1 ) );

   // Amount must be positive
   REQUIRE_OP_VALIDATION_SUCCESS( op, amount, core.amount(  1 ) );
   REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount(  0 ) );
   REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount( -1 ) );

   // Setup world state we will need to test actual evaluation
   const account_object& alice_account = create_account( "alice" );
   const account_object& bob_account = create_account( "bob" );

   transfer( committee_account(db), alice_account, core.amount( 1000000 ) );

   auto spin_vbo_clock = [&]( const vesting_balance_object& vbo, uint32_t dt_secs )
   {
      // HACK:  This just modifies the DB creation record to be further
      //    in the past
      db.modify( vbo, [&]( vesting_balance_object& _vbo )
      {
         _vbo.policy.get<cdd_vesting_policy>().coin_seconds_earned_last_update -= dt_secs;
      } );
   };

   auto create_vbo = [&](
      account_id_type creator, account_id_type owner,
      asset amount, uint32_t vesting_seconds, uint32_t elapsed_seconds
      ) -> const vesting_balance_object&
   {
      transaction tx;

      vesting_balance_create_operation create_op;
      create_op.fee = core.amount( 0 );
      create_op.creator = creator;
      create_op.owner = owner;
      create_op.amount = amount;
      create_op.policy = cdd_vesting_policy_initializer(vesting_seconds);
      tx.operations.push_back( create_op );
      set_expiration( db, tx );

      processed_transaction ptx = PUSH_TX( db,  tx, ~0  );
      const vesting_balance_object& vbo = vesting_balance_id_type(
         ptx.operation_results[0].get<object_id_type>())(db);

      if( elapsed_seconds > 0 )
         spin_vbo_clock( vbo, elapsed_seconds );
      return vbo;
   };

   auto top_up = [&]()
   {
      trx.clear();
      transfer( committee_account(db),
         alice_account,
         core.amount( 1000000 - db.get_balance( alice_account, core ).amount )
         );
      FC_ASSERT( db.get_balance( alice_account, core ).amount == 1000000 );
      trx.clear();
      trx.operations.push_back( op );
   };

   trx.clear();
   trx.operations.push_back( op );

   {
      // Try withdrawing a single satoshi
      const vesting_balance_object& vbo = create_vbo(
         alice_account.id, alice_account.id, core.amount( 10000 ), 1000, 0);

      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990000 );

      op.vesting_balance = vbo.id;
      op.owner = alice_account.id;

      REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(1) );

      // spin the clock and make sure we can withdraw 1/1000 in 1 second
      spin_vbo_clock( vbo, 1 );
      // Alice shouldn't be able to withdraw 11, it's too much
      REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(11) );
      op.amount = core.amount( 1 );
      // Bob shouldn't be able to withdraw anything
      REQUIRE_THROW_WITH_VALUE( op, owner, bob_account.id );
      // Shouldn't be able to get out different asset than was put in
      REQUIRE_THROW_WITH_VALUE( op, amount, test_asset.amount(1) );
      // Withdraw the max, we are OK...
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(10) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990010 );
      top_up();
   }

   // Make sure we can withdraw the correct amount after 999 seconds
   {
      const vesting_balance_object& vbo = create_vbo(
         alice_account.id, alice_account.id, core.amount( 10000 ), 1000, 999);

      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990000 );

      op.vesting_balance = vbo.id;
      op.owner = alice_account.id;
      // Withdraw one satoshi too much, no dice
      REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(9991) );
      // Withdraw just the right amount, success!
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(9990) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  999990 );
      top_up();
   }

   // Make sure we can withdraw the whole thing after 1000 seconds
   {
      const vesting_balance_object& vbo = create_vbo(
         alice_account.id, alice_account.id, core.amount( 10000 ), 1000, 1000);

      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990000 );

      op.vesting_balance = vbo.id;
      op.owner = alice_account.id;
      // Withdraw one satoshi too much, no dice
      REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(10001) );
      // Withdraw just the right amount, success!
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(10000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount == 1000000 );
   }

   // Make sure that we can't withdraw a single extra satoshi no matter how old it is
   {
      const vesting_balance_object& vbo = create_vbo(
         alice_account.id, alice_account.id, core.amount( 10000 ), 1000, 123456);

      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990000 );

      op.vesting_balance = vbo.id;
      op.owner = alice_account.id;
      // Withdraw one satoshi too much, no dice
      REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(10001) );
      // Withdraw just the right amount, success!
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(10000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount == 1000000 );
   }

   // Try withdrawing in three max installments:
   //   5000 after  500      seconds
   //   2000 after  400 more seconds
   //   3000 after 1000 more seconds
   {
      const vesting_balance_object& vbo = create_vbo(
         alice_account.id, alice_account.id, core.amount( 10000 ), 1000, 0);

      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990000 );

      op.vesting_balance = vbo.id;
      op.owner = alice_account.id;
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(   1) );
      spin_vbo_clock( vbo, 499 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(5000) );
      spin_vbo_clock( vbo,   1 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(5001) );
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(5000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  995000 );

      spin_vbo_clock( vbo, 399 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(2000) );
      spin_vbo_clock( vbo,   1 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(2001) );
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(2000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  997000 );

      spin_vbo_clock( vbo, 999 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(3000) );
      spin_vbo_clock( vbo, 1   );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(3001) );
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(3000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount == 1000000 );
   }

   //
   // Increase by 10,000 csd / sec initially.
   // After 500 seconds, we have 5,000,000 csd.
   // Withdraw 2,000, we are now at 8,000 csd / sec.
   // At 8,000 csd / sec, it will take us 625 seconds to mature.
   //
   {
      const vesting_balance_object& vbo = create_vbo(
         alice_account.id, alice_account.id, core.amount( 10000 ), 1000, 0);

      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  990000 );

      op.vesting_balance = vbo.id;
      op.owner = alice_account.id;
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(   1) );
      spin_vbo_clock( vbo, 500 );
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(2000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount ==  992000 );

      spin_vbo_clock( vbo, 624 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(8000) );
      spin_vbo_clock( vbo,   1 );
      REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(8001) );
      REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(8000) );
      FC_ASSERT( db.get_balance( alice_account,       core ).amount == 1000000 );
   }
   // TODO:  Test with non-core asset and Bob account
} FC_LOG_AND_RETHROW() }

// TODO:  Write linear VBO tests

BOOST_AUTO_TEST_SUITE_END()
