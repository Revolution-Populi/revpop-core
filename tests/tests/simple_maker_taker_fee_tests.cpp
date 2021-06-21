/*
 * Copyright (c) 2020 Michel Santos, and contributors.
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

#include <string>
#include <boost/test/unit_test.hpp>
#include <fc/exception/exception.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/market_object.hpp>

#include "../common/database_fixture.hpp"


using namespace graphene::chain;
using namespace graphene::chain::test;

struct simple_maker_taker_database_fixture : database_fixture {
   simple_maker_taker_database_fixture()
           : database_fixture() {
   }

   const asset_create_operation create_user_issued_asset_operation(const string &name, const account_object &issuer,
                                                                   uint16_t flags, const price &core_exchange_rate,
                                                                   uint8_t precision, uint16_t maker_fee_percent,
                                                                   uint16_t taker_fee_percent) {
      asset_create_operation creator;
      creator.issuer = issuer.id;
      creator.fee = asset();
      creator.symbol = name;
      creator.common_options.max_supply = 0;
      creator.precision = precision;

      creator.common_options.core_exchange_rate = core_exchange_rate;
      creator.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      creator.common_options.flags = flags;
      creator.common_options.issuer_permissions = flags;
      creator.common_options.market_fee_percent = maker_fee_percent;
      creator.common_options.extensions.value.taker_fee_percent = taker_fee_percent;

      return creator;

   }
};


/**
 * BSIP81: Asset owners may specify different market fee rate for maker orders and taker orders
 */
BOOST_FIXTURE_TEST_SUITE(simple_maker_taker_fee_tests, simple_maker_taker_database_fixture)

   /**
    * Test of setting taker fee before HF and after HF for a UIA
    */
   BOOST_AUTO_TEST_CASE(setting_taker_fees_uia) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy));
         account_id_type issuer_id = jill.id;
         fc::ecc::private_key issuer_private_key = jill_private_key;

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));
         uint16_t market_fee_percent = 20 * GRAPHENE_1_PERCENT;
         const asset_object jillcoin = create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                market_fee_percent);

         //////
         // Test inability to set taker fees
         //////
         asset_update_operation uop;
         uop.issuer = issuer_id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options = jillcoin.options;
         uint16_t new_taker_fee_percent = uop.new_options.market_fee_percent / 2;
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;

         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, issuer_private_key);
         PUSH_TX(db, trx);

         // Check the taker fee
         asset_object updated_asset = jillcoin.get_id()(db);
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());

         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test default values of taker fee after HF
         // After the HF its default value should still not be set
         //////
         updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = updated_asset.options.market_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, test invalid taker fees
         //////
         uop.new_options.extensions.value.taker_fee_percent = GRAPHENE_100_PERCENT + 1;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, issuer_private_key);
         GRAPHENE_CHECK_THROW(PUSH_TX(db, trx), fc::exception); // An exception should be thrown indicating the reason
         // TODO: Check the specific exception?


         //////
         // After HF, test that new values can be set
         //////
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, issuer_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee
         updated_asset = jillcoin.get_id()(db);
         expected_taker_fee_percent = new_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);


         //////
         // After HF, test ability to set taker fees with an asset update operation inside of a proposal
         //////
         {
            trx.clear();
            set_expiration(db, trx);

            uint64_t alternate_taker_fee_percent = new_taker_fee_percent * 2;
            uop.new_options.extensions.value.taker_fee_percent = alternate_taker_fee_percent;

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(uop);

            trx.operations.push_back(cop);
            // sign(trx, issuer_private_key);
            processed_transaction processed = PUSH_TX(db, trx); // No exception should be thrown

            // Check the taker fee is not changed because the proposal has not been approved
            updated_asset = jillcoin.get_id()(db);
            expected_taker_fee_percent = new_taker_fee_percent;
            BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
            BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);


            // Approve the proposal
            trx.clear();
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = jill.id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(jill.id);
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, jill_private_key);

            PUSH_TX(db, trx); // No exception should be thrown

            // Advance to after proposal expires
            generate_blocks(cop.expiration_time);

            // Check the taker fee is not changed because the proposal has not been approved
            updated_asset = jillcoin.get_id()(db);
            expected_taker_fee_percent = alternate_taker_fee_percent;
            BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
            BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         }


         //////
         // After HF, test ability to set taker fees with an asset create operation inside of a proposal
         //////
         {
            trx.clear();
            set_expiration(db, trx);

            uint64_t maker_fee_percent = 10 * GRAPHENE_1_PERCENT;
            uint64_t taker_fee_percent = 2 * GRAPHENE_1_PERCENT;
            asset_create_operation ac_op = create_user_issued_asset_operation("JCOIN2", jill, charge_market_fee, price,
                                                                              2,
                                                                              maker_fee_percent, taker_fee_percent);

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(ac_op);

            trx.operations.push_back(cop);
            // sign(trx, issuer_private_key);

            processed_transaction processed = PUSH_TX(db, trx); // No exception should be thrown

            // Check the asset does not exist because the proposal has not been approved
            const auto& asset_idx = db.get_index_type<asset_index>().indices().get<by_symbol>();
            const auto itr = asset_idx.find("JCOIN2");
            BOOST_CHECK(itr == asset_idx.end());

            // Approve the proposal
            trx.clear();
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = jill.id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(jill.id);
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, jill_private_key);

            PUSH_TX(db, trx); // No exception should be thrown

            // Advance to after proposal expires
            generate_blocks(cop.expiration_time);

            // Check the taker fee is not changed because the proposal has not been approved
            BOOST_CHECK(asset_idx.find("JCOIN2") != asset_idx.end());
            updated_asset = *asset_idx.find("JCOIN2");
            expected_taker_fee_percent = taker_fee_percent;
            BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
            BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);
            uint16_t expected_maker_fee_percent = maker_fee_percent;
            BOOST_CHECK_EQUAL(expected_maker_fee_percent, updated_asset.options.market_fee_percent);

         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of setting taker fee before HF and after HF for a smart asset
    */
   BOOST_AUTO_TEST_CASE(setting_taker_fees_smart_asset) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((smartissuer)(feedproducer));

         // Initialize tokens
         create_bitasset("SMARTBIT", smartissuer.id);
         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object &bitsmart = get_asset("SMARTBIT");

         generate_block();


         //////
         // Before HF, test inability to set taker fees
         //////
         asset_update_operation uop;
         uop.issuer = smartissuer.id;
         uop.asset_to_update = bitsmart.get_id();
         uop.new_options = bitsmart.options;
         uint16_t new_taker_fee_percent = uop.new_options.market_fee_percent / 2;
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;

         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, smartissuer_private_key);
         PUSH_TX(db, trx);

         // Check the taker fee
         asset_object updated_asset = bitsmart.get_id()(db);
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, test default values of taker fee after HF
         // After the HF its default value should still not be set
         //////
         updated_asset = bitsmart.get_id()(db);
         uint16_t expected_taker_fee_percent = updated_asset.options.market_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, test invalid taker fees
         //////
         uop.new_options.extensions.value.taker_fee_percent = GRAPHENE_100_PERCENT + 1;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, smartissuer_private_key);
         GRAPHENE_CHECK_THROW(PUSH_TX(db, trx), fc::exception); // An exception should be thrown indicating the reason
         // TODO: Check the specific exception?


         //////
         // After HF, test that new values can be set
         //////
         new_taker_fee_percent = uop.new_options.market_fee_percent / 4;
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, smartissuer_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee
         updated_asset = bitsmart.get_id()(db);
         expected_taker_fee_percent = new_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test the default taker fee values of multiple different assets after HF
    */
   BOOST_AUTO_TEST_CASE(default_taker_fees) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((alice)(bob)(charlie)(smartissuer));

         // Initialize tokens with custom market fees
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t alice1coin_market_fee_percent = 1 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ALICE1COIN", alice, charge_market_fee, price, 2,
                                                                  alice1coin_market_fee_percent);

         const uint16_t alice2coin_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ALICE2COIN", alice, charge_market_fee, price, 2,
                                                                  alice2coin_market_fee_percent);

         const uint16_t bob1coin_market_fee_percent = 3 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("BOB1COIN", alice, charge_market_fee, price, 2,
                                                                bob1coin_market_fee_percent);

         const uint16_t bob2coin_market_fee_percent = 4 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("BOB2COIN", alice, charge_market_fee, price, 2,
                                                                bob2coin_market_fee_percent);

         const uint16_t charlie1coin_market_fee_percent = 4 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("CHARLIE1COIN", alice, charge_market_fee, price, 2,
                                                                    charlie1coin_market_fee_percent);

         const uint16_t charlie2coin_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("CHARLIE2COIN", alice, charge_market_fee, price, 2,
                                                                    charlie2coin_market_fee_percent);

         const uint16_t bitsmart1coin_market_fee_percent = 7 * GRAPHENE_1_PERCENT;
         create_bitasset("SMARTBIT1", smartissuer.id, bitsmart1coin_market_fee_percent);


         const uint16_t bitsmart2coin_market_fee_percent = 8 * GRAPHENE_1_PERCENT;
         create_bitasset("SMARTBIT2", smartissuer.id, bitsmart2coin_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& alice1coin = get_asset("ALICE1COIN");
         const asset_object& alice2coin = get_asset("ALICE2COIN");
         const asset_object& bob1coin = get_asset("BOB1COIN");
         const asset_object& bob2coin = get_asset("BOB2COIN");
         const asset_object& charlie1coin = get_asset("CHARLIE1COIN");
         const asset_object& charlie2coin = get_asset("CHARLIE2COIN");
         const asset_object& bitsmart1 = get_asset("SMARTBIT1");
         const asset_object& bitsmart2 = get_asset("SMARTBIT2");


         //////
         // Before HF, test the market/maker fees for each asset
         //////
         asset_object updated_asset;
         uint16_t expected_fee_percent;

         updated_asset = alice1coin.get_id()(db);
         expected_fee_percent = alice1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = alice2coin.get_id()(db);
         expected_fee_percent = alice2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob1coin.get_id()(db);
         expected_fee_percent = bob1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob2coin.get_id()(db);
         expected_fee_percent = bob2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie1coin.get_id()(db);
         expected_fee_percent = charlie1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie2coin.get_id()(db);
         expected_fee_percent = charlie2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bitsmart1.get_id()(db);
         expected_fee_percent = bitsmart1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bitsmart2.get_id()(db);
         expected_fee_percent = bitsmart2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);


         //////
         // Before HF, test that taker fees are not set
         //////
         // Check the taker fee
         updated_asset = alice1coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = alice2coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob1coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob2coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie1coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie2coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bitsmart1.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bitsmart2.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test the maker fees for each asset are unchanged
         //////
         updated_asset = alice1coin.get_id()(db);
         expected_fee_percent = alice1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = alice2coin.get_id()(db);
         expected_fee_percent = alice2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob1coin.get_id()(db);
         expected_fee_percent = bob1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob2coin.get_id()(db);
         expected_fee_percent = bob2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie1coin.get_id()(db);
         expected_fee_percent = charlie1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie2coin.get_id()(db);
         expected_fee_percent = charlie2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bitsmart1.get_id()(db);
         expected_fee_percent = bitsmart1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bitsmart2.get_id()(db);
         expected_fee_percent = bitsmart2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);


         //////
         // After HF, test the taker fees for each asset are not set
         //////
         updated_asset = alice1coin.get_id()(db);
         expected_fee_percent = alice1coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = alice2coin.get_id()(db);
         expected_fee_percent = alice2coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob1coin.get_id()(db);
         expected_fee_percent = bob1coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob2coin.get_id()(db);
         expected_fee_percent = bob2coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie1coin.get_id()(db);
         expected_fee_percent = charlie1coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie2coin.get_id()(db);
         expected_fee_percent = charlie2coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bitsmart1.get_id()(db);
         expected_fee_percent = bitsmart1coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bitsmart2.get_id()(db);
         expected_fee_percent = bitsmart2coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a UIA
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_1) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", izzy, charge_market_fee, price, 3,
                                                                izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = jill_maker_fee_percent / 2;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         uint16_t izzy_taker_fee_percent = izzy_maker_fee_percent / 2;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = jill.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, jill_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Set the new taker fee for IZZYCOIN
         uop.issuer = izzy.id;
         uop.asset_to_update = izzycoin.get_id();
         uop.new_options = izzycoin.options;
         uop.new_options.extensions.value.taker_fee_percent = izzy_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, izzy_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for IZZYCOIN
         updated_asset = izzycoin.get_id()(db);
         expected_taker_fee_percent = izzy_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a UIA
    *
    * Test the filling of a taker fee when the **maker** fee percent is set to 0.  This tests some optimizations
    * in database::calculate_market_fee().
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_2) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 0 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 0 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", izzy, charge_market_fee, price, 3,
                                                                izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = 1 * GRAPHENE_1_PERCENT;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         uint16_t izzy_taker_fee_percent = 3 * GRAPHENE_1_PERCENT;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = jill.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options.market_fee_percent = jill_maker_fee_percent;
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, jill_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Set the new taker fee for IZZYCOIN
         uop.issuer = izzy.id;
         uop.asset_to_update = izzycoin.get_id();
         uop.new_options.market_fee_percent = izzy_maker_fee_percent;
         uop.new_options = izzycoin.options;
         uop.new_options.extensions.value.taker_fee_percent = izzy_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, izzy_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for IZZYCOIN
         updated_asset = izzycoin.get_id()(db);
         expected_taker_fee_percent = izzy_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a UIA
    *
    * Test the filling of a taker fee when the **taker** fee percent is set to 0.  This tests some optimizations
    * in database::calculate_market_fee().
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_3) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", izzy, charge_market_fee, price, 3,
                                                                izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = 0 * GRAPHENE_1_PERCENT;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         uint16_t izzy_taker_fee_percent = 0 * GRAPHENE_1_PERCENT;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = jill.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options.market_fee_percent = jill_maker_fee_percent;
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, jill_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Set the new taker fee for IZZYCOIN
         uop.issuer = izzy.id;
         uop.asset_to_update = izzycoin.get_id();
         uop.new_options.market_fee_percent = izzy_maker_fee_percent;
         uop.new_options = izzycoin.options;
         uop.new_options.extensions.value.taker_fee_percent = izzy_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, izzy_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for IZZYCOIN
         updated_asset = izzycoin.get_id()(db);
         expected_taker_fee_percent = izzy_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of **default** taker fees charged when filling limit orders after HF for a UIA.
    *
    * This test is similar to simple_match_and_fill_with_different_fees_uia_1
    * except that the taker fee is not explicitly set and instead defaults to the maker fee.
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_4) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                  jill_market_fee_percent);

         const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", izzy, charge_market_fee, price, 3,
                                  izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that default taker values has not been set
         //////
         // The taker fees should automatically default to maker fees if the taker fee is not explicitly set
         // UNUSED: uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         // UNUSED: uint16_t jill_taker_fee_percent = jill_market_fee_percent;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         // UNUSED: uint16_t izzy_taker_fee_percent = izzy_market_fee_percent;

         // Check the taker fee for JCOIN: it should still not be set
         asset_object updated_asset = jillcoin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         // Check the taker fee for ICOIN: it should still not be set
         updated_asset = izzycoin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, create limit orders that will perfectly match
         //////
         BOOST_TEST_MESSAGE("Issuing 10 jillcoin to alice");
         issue_uia(alice, jillcoin.amount(10 * JILL_PRECISION));
         BOOST_TEST_MESSAGE("Checking alice's balance");
         BOOST_REQUIRE_EQUAL(get_balance(alice, jillcoin), 10 * JILL_PRECISION);

         BOOST_TEST_MESSAGE("Issuing 300 izzycoin to bob");
         issue_uia(bob, izzycoin.amount(300 * IZZY_PRECISION));
         BOOST_TEST_MESSAGE("Checking bob's balance");
         BOOST_REQUIRE_EQUAL(get_balance(bob, izzycoin), 300 * IZZY_PRECISION);

      } FC_LOG_AND_RETHROW()
   }



   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a smart asset
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_smart_asset) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob));
         ACTORS((smartissuer)(feedproducer));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));
         const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         const asset_object jillcoin = create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         const uint16_t smartbit_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_bitasset("SMARTBIT", smartissuer.id, smartbit_market_fee_percent,
                                                        charge_market_fee, 4);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object &smartbit = get_asset("SMARTBIT");

         const auto &core = asset_id_type()(db);

         update_feed_producers(smartbit, {feedproducer.id});

         price_feed current_feed;
         current_feed.settlement_price = smartbit.amount(100) / core.amount(100);
         current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
         publish_feed(smartbit, feedproducer, current_feed);

         FC_ASSERT(smartbit.bitasset_data(db).current_feed.settlement_price == current_feed.settlement_price);


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = jill_maker_fee_percent / 2;

         uint16_t smartbit_maker_fee_percent = 1 * GRAPHENE_1_PERCENT;
         uint16_t smartbit_taker_fee_percent = 3 * GRAPHENE_1_PERCENT;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = jill.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, jill_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);


         // Set the new taker fee for SMARTBIT
         uop = asset_update_operation();
         uop.issuer = smartissuer.id;
         uop.asset_to_update = smartbit.get_id();
         uop.new_options = smartbit.options;
         uop.new_options.market_fee_percent = smartbit_maker_fee_percent;
         uop.new_options.extensions.value.taker_fee_percent = smartbit_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, smartissuer_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for SMARTBIT
         updated_asset = smartbit.get_id()(db);
         expected_taker_fee_percent = smartbit_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Check the maker fee for SMARTBIT
         updated_asset = smartbit.get_id()(db);
         expected_taker_fee_percent = smartbit_maker_fee_percent;
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, updated_asset.options.market_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a smart asset
    * and a user-issued asset
    *
    * 1. (Order 1) An order will be placed to offer JCOIN
    *
    * 2. (Order 2) A matching-order will be placed to offer SMARTBIT.
    *     Order 2 is large enough that it should be partially filled, and Order 1 will be completely filled.
    *     Order 1 should be charged a maker fee, and Order 2 should be charged a taker fee.
    *     Order 2 should remain on the book.
    *
    * 3. (Order 3) A matching order will be placed to offer JCOIN.
    *     Order 3 should be charged a taker fee, and Order 2 should be charged a maker fee.
    *
    * Summary: Order 2 should be charged a taker fee when matching Order 1,
    * and Order 2 should be charged a maker fee when matching Order 3.
    */
   BOOST_AUTO_TEST_CASE(partial_maker_partial_taker_fills_1) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob)(charlie));
         ACTORS((smartissuer)(feedproducer));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         const asset_object jillcoin = create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         const uint16_t smartbit_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_bitasset("SMARTBIT", smartissuer.id, smartbit_market_fee_percent,
                                                        charge_market_fee, 4);
         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object &smartbit = get_asset("SMARTBIT");

         const auto &core = asset_id_type()(db);

         update_feed_producers(smartbit, {feedproducer.id});

         price_feed current_feed;
         current_feed.settlement_price = smartbit.amount(100) / core.amount(100);
         current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
         publish_feed(smartbit, feedproducer, current_feed);

         FC_ASSERT(smartbit.bitasset_data(db).current_feed.settlement_price == current_feed.settlement_price);


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = jill_maker_fee_percent / 2;

         uint16_t smartbit_maker_fee_percent = 1 * GRAPHENE_1_PERCENT;
         uint16_t smartbit_taker_fee_percent = 3 * GRAPHENE_1_PERCENT;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = jill.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, jill_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Check the maker fee for JILLCOIN
         uint16_t expected_maker_fee_percent = jill_maker_fee_percent;
         BOOST_CHECK_EQUAL(expected_maker_fee_percent, updated_asset.options.market_fee_percent);


         // Set the new taker fee for SMARTBIT
         uop = asset_update_operation();
         uop.issuer = smartissuer.id;
         uop.asset_to_update = smartbit.get_id();
         uop.new_options = smartbit.options;
         uop.new_options.market_fee_percent = smartbit_maker_fee_percent;
         uop.new_options.extensions.value.taker_fee_percent = smartbit_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, smartissuer_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for SMARTBIT
         updated_asset = smartbit.get_id()(db);
         expected_taker_fee_percent = smartbit_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Check the maker fee for SMARTBIT
         updated_asset = smartbit.get_id()(db);
         expected_taker_fee_percent = smartbit_maker_fee_percent;
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, updated_asset.options.market_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of **default** taker fees charged when filling limit orders after HF for a smart asset
    * and a user-issued asset
    *
    * This test is similar to partial_maker_partial_taker_fills_1 except that
    * (a) the taker fee is not explicitly set and instead defaults to the maker fee, and
    * (b) Orders 1 and 2 are placed before the HF and Order 3 is placed after the HF.
    *
    * 1. (Order 1) An order will be placed to offer JCOIN
    *
    * 2. (Order 2) A matching-order will be placed to offer SMARTBIT.
    *     Order 2 is large enough that it should be partially filled, and Order 1 will be completely filled.
    *     Order 1 should be charged a maker fee, and Order 2 should be charged a taker fee.
    *     Order 2 should remain on the book.
    *
    * 3. (Order 3) A matching order will be placed to offer JCOIN.
    *     Order 3 should be charged a taker fee, and Order 2 should be charged a maker fee.
    *
    * Summary: Order 2 should be charged a taker fee when matching Order 1,
    * and Order 2 should be charged a maker fee when matching Order 3.
    */
   BOOST_AUTO_TEST_CASE(partial_maker_partial_taker_fills_2) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob)(charlie));
         ACTORS((smartissuer)(feedproducer));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         const asset_object jillcoin = create_user_issued_asset("JCOIN", jill, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         const uint16_t smartbit_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_bitasset("SMARTBIT", smartissuer.id, smartbit_market_fee_percent,
                         charge_market_fee, 4);

         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = jill_market_fee_percent;

         uint16_t smartbit_maker_fee_percent = smartbit_market_fee_percent;


         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object &smartbit = get_asset("SMARTBIT");

         const auto &core = asset_id_type()(db);

         update_feed_producers(smartbit, {feedproducer.id});

         price_feed current_feed;
         current_feed.settlement_price = smartbit.amount(100) / core.amount(100);
         current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
         publish_feed(smartbit, feedproducer, current_feed);

         FC_ASSERT(smartbit.bitasset_data(db).current_feed.settlement_price == current_feed.settlement_price);


         //////
         // After HF, The taker fees should automatically default to maker fees when the taker fee is not explicitly set
         //////
         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         // Check the maker fee for JILLCOIN
         uint16_t expected_maker_fee_percent = jill_maker_fee_percent;
         BOOST_CHECK_EQUAL(expected_maker_fee_percent, updated_asset.options.market_fee_percent);

         // Check the taker fee for SMARTBIT
         updated_asset = smartbit.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         // Check the maker fee for SMARTBIT
         expected_maker_fee_percent = smartbit_maker_fee_percent;
         BOOST_CHECK_EQUAL(expected_maker_fee_percent, updated_asset.options.market_fee_percent);

      } FC_LOG_AND_RETHROW()
   }

BOOST_AUTO_TEST_SUITE_END()
