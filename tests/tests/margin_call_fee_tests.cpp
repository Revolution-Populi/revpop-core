/*
 * Copyright (c) 2020 Contributors
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

#include <graphene/chain/hardfork.hpp>

#include "../common/database_fixture.hpp"

// For account history
#include <vector>
#include <graphene/app/api.hpp>


using namespace graphene::chain;
using namespace graphene::chain::test;

struct bitasset_database_fixture : database_fixture {
   bitasset_database_fixture()
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


BOOST_FIXTURE_TEST_SUITE(margin_call_fee_tests, bitasset_database_fixture)

   /**
    * Test the effects of different MCFRs on derived prices and ratios
    */
   BOOST_AUTO_TEST_CASE(mcfr_tests) {
      try {
         ACTORS((charlie))
         const asset_id_type core_id;

         BOOST_TEST_MESSAGE("Advancing past Hardfork BSIP74");
         generate_blocks(HARDFORK_CORE_BSIP74_TIME);
         generate_block();
         set_expiration(db, trx);

         //////
         // Initialize
         //////
         fc::optional<uint16_t> mcfr;
         price expected_offer_price; // The price offered by the margin call
         price expected_paid_price; // The effective price paid by the margin call
         ratio_type expected_margin_call_pays_ratio;

         const asset_object core = core_id(db);
         const uint16_t smartbit_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_bitasset("SMARTBIT2", charlie_id, smartbit_market_fee_percent, charge_market_fee, 2);
         generate_block();
         const asset_object smartbit2 = get_asset("SMARTBIT2");
         BOOST_CHECK_EQUAL(2, smartbit2.precision);

         // Construct a price feed
         // Initial price of 1 satoshi SMARTBIT2 for 20 satoshi CORE
         // = 0.0001 SMARTBIT2 for 0.00020 CORE = 1 SMARTBIT2 for 2 CORE
         const price initial_price =
                 smartbit2.amount(1) / core.amount(20); // 1 satoshi SMARTBIT2 for 20 satoshi CORE

         price_feed feed;
         feed.settlement_price = initial_price;
         feed.maintenance_collateral_ratio = 1750; // MCR of 1.75x
         feed.maximum_short_squeeze_ratio = 1500; // MSSR of 1.50x

         //////
         // Check prices and ratios when MSSR = 150% and MCFR is not set
         //////
         mcfr = {};

         // Expected paid price = price / MSSR
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1500 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.max_short_squeeze_price());

         // Expected offer price = price / (MSSR-MCFR)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / ([1500 - 0] / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.margin_call_order_price(mcfr));

         // Expected margin call pays ratio = (MSSR-MCFR) / MSSR
         // = (1500 - 0) / 1500
         // = 1
         expected_margin_call_pays_ratio = ratio_type(1, 1);
         BOOST_CHECK(expected_margin_call_pays_ratio == feed.margin_call_pays_ratio(mcfr));


         //////
         // Check prices and ratios when MSSR = 150% and MCFR = 0
         //////
         mcfr = 0;

         // Expected paid price = price / MSSR
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1500 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.max_short_squeeze_price());

         // Expected offer price = price / (MSSR-MCFR)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / ([1500 - 0] / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.margin_call_order_price(mcfr));

         // Expected margin call pays ratio = (MSSR-MCFR) / MSSR
         // = (1500 - 0) / 1500
         // = 1
         expected_margin_call_pays_ratio = ratio_type(1, 1);
         BOOST_CHECK(expected_margin_call_pays_ratio == feed.margin_call_pays_ratio(mcfr));


         //////
         // Check prices and ratios when MSSR = 150% and MCFR = 5%
         //////
         mcfr = 50; // 5% expressed in terms of GRAPHENE_COLLATERAL_RATIO_DENOM

         // Expected paid price = price / MSSR
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1500 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.max_short_squeeze_price());

         // Expected offer price = price / (MSSR-MCFR)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / ([1500 - 50] / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1450 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (145 / 100)
         // = (1 satoshi SMARTBIT2 / 2 satoshi Core) / (145 / 10)
         // = (10 satoshi SMARTBIT2 / 290 satoshi Core)
         // = (1 satoshi SMARTBIT2 / 29 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(29));
         BOOST_CHECK(expected_paid_price == feed.margin_call_order_price(mcfr));

         // Expected margin call pays ratio = (MSSR-MCFR) / MSSR
         // = (1500 - 50) / 1500
         // = 1450 / 1500 = 145 / 150 = 29 / 30
         expected_margin_call_pays_ratio = ratio_type(29, 30);
         BOOST_CHECK(expected_margin_call_pays_ratio == feed.margin_call_pays_ratio(mcfr));


         //////
         // Check prices and ratios when MSSR = 150% and MCFR = 30%
         //////
         mcfr = 300; // 30% expressed in terms of GRAPHENE_COLLATERAL_RATIO_DENOM

         // Expected paid price = price / MSSR
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1500 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.max_short_squeeze_price());

         // Expected offer price = price / (MSSR-MCFR)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / ([1500 - 300] / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1200 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (6 / 5)
         // = (5 satoshi SMARTBIT2 / 120 satoshi Core)
         // = (1 satoshi SMARTBIT2 / 24 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(24));
         BOOST_CHECK(expected_paid_price == feed.margin_call_order_price(mcfr));

         // Expected margin call pays ratio = (MSSR-MCFR) / MSSR
         // = (1500 - 300) / 1500
         // = 1200 / 1500 = 4 / 5
         expected_margin_call_pays_ratio = ratio_type(4, 5);
         BOOST_CHECK(expected_margin_call_pays_ratio == feed.margin_call_pays_ratio(mcfr));


         //////
         // Check prices and ratios when MSSR = 150% and MCFR = 60%
         //////
         mcfr = 600; // 60% expressed in terms of GRAPHENE_COLLATERAL_RATIO_DENOM

         // Expected paid price = price / MSSR
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (1500 / 1000)
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core) / (3 / 2)
         // = (1 satoshi SMARTBIT2 / 30 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(30));
         BOOST_CHECK(expected_paid_price == feed.max_short_squeeze_price());

         // Expected offer price = price / (MSSR-MCFR)
         // but (MSSR-MCFR) has a floor 1
         // Therefore = price / 1 = price
         // = (1 satoshi SMARTBIT2 / 20 satoshi Core)
         expected_paid_price = price(smartbit2.amount(1), core.amount(20));
         BOOST_CHECK(expected_paid_price == feed.margin_call_order_price(mcfr));

         // Expected margin call pays ratio = (MSSR-MCFR) / MSSR
         // but (MSSR-MCFR) has a floor 1
         // Therefore = 1 / MSSR
         // = 1000 / 1500 = 2 / 3
         expected_margin_call_pays_ratio = ratio_type(2, 3);
         BOOST_CHECK(expected_margin_call_pays_ratio == feed.margin_call_pays_ratio(mcfr));

      }
      FC_LOG_AND_RETHROW()
   }


   /**
    * Test the ability to create and update assets with a margin call fee ratio (MCFR) before HARDFORK_CORE_BSIP74_TIME
    *
    *
    * Before HARDFORK_CORE_BSIP74_TIME
    *
    * 1. Asset owner fails to create the smart coin called USDBIT with a MCFR
    * 2. Asset owner fails to create the smart coin called USDBIT with a MCFR in a proposal
    * 3. Asset owner succeeds to create the smart coin called USDBIT without a MCFR
    *
    * 4. Asset owner fails to update the smart coin with a MCFR
    * 5. Asset owner fails to update the smart coin with a MCFR in a proposal
    *
    *
    * 6. Activate HARDFORK_CORE_BSIP74_TIME
    *
    *
    * After HARDFORK_CORE_BSIP74_TIME
    *
    * 7. Asset owner succeeds to create the smart coin called CNYBIT with a MCFR
    * 8. Asset owner succeeds to create the smart coin called RUBBIT with a MCFR in a proposal
    *
    * 9. Asset owner succeeds to update the smart coin called CNYBIT with a MCFR
    * 10. Asset owner succeeds to update the smart coin called RUBBIT with a MCFR in a proposal
    *
    * 11. Asset owner succeeds to create the smart coin called YENBIT without a MCFR
    * 12. Asset owner succeeds to update the smart coin called RUBBIT without a MCFR in a proposal
    */
   BOOST_AUTO_TEST_CASE(prevention_before_hardfork_test) {
      try {
         ///////
         // Initialize the scenario
         ///////
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
         trx.clear();
         set_expiration(db, trx);

         // Create actors
         ACTORS((assetowner));

         // CORE asset exists by default
         asset_object core = asset_id_type()(db);
         const asset_id_type core_id = core.id;

         // Fund actors
         uint64_t initial_balance_core = 10000000;
         transfer(committee_account, assetowner_id, asset(initial_balance_core));

         // Confirm before hardfork activation
         BOOST_CHECK(db.head_block_time() < HARDFORK_CORE_BSIP74_TIME);


         ///////
         // 1. Asset owner fails to create the smart coin called bitUSD with a MCFR
         ///////
         const uint16_t market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         const optional<uint16_t> icr_opt = {}; // Initial collateral ratio
         const uint16_t mcfr_5 = 50; // 5% expressed in terms of GRAPHENE_COLLATERAL_RATIO_DENOM
         optional<uint16_t> mcfr_opt = mcfr_5;

         // Attempt to create the smart asset with a MCFR
         // The attempt should fail because it is before HARDFORK_CORE_BSIP74_TIME
         {
            const asset_create_operation create_op = make_bitasset("USDBIT", assetowner_id, market_fee_percent,
                                                                   charge_market_fee, 4, core_id,
                                                                   GRAPHENE_MAX_SHARE_SUPPLY, icr_opt, mcfr_opt);
            trx.clear();
            trx.operations.push_back(create_op);
            sign(trx, assetowner_private_key);
            REQUIRE_EXCEPTION_WITH_TEXT(PUSH_TX(db, trx), "cannot be set before Hardfork BSIP74");
         }

         ///////
         // 2. Asset owner fails to create the smart coin called bitUSD with a MCFR in a proposal
         ///////
         {
            const asset_create_operation create_op = make_bitasset("USDBIT", assetowner_id, market_fee_percent,
                                                                   charge_market_fee, 4, core_id,
                                                                   GRAPHENE_MAX_SHARE_SUPPLY, icr_opt, mcfr_opt);
            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(create_op);

            trx.clear();
            trx.operations.push_back(cop);
            // sign(trx, assetowner_private_key);
            REQUIRE_EXCEPTION_WITH_TEXT(PUSH_TX(db, trx), "cannot be set before Hardfork BSIP74");
         }


         ///////
         // 3. Asset owner succeeds to create the smart coin called bitUSD without a MCFR
         ///////
         const optional<uint16_t> mcfr_null_opt = {};
         {
            const asset_create_operation create_op = make_bitasset("USDBIT", assetowner_id, market_fee_percent,
                                                                   charge_market_fee, 4, core_id,
                                                                   GRAPHENE_MAX_SHARE_SUPPLY, icr_opt, mcfr_null_opt);

            trx.clear();
            trx.operations.push_back(create_op);
            sign(trx, assetowner_private_key);
            PUSH_TX(db, trx); // No exception should be thrown
         }

         generate_block();
         set_expiration(db, trx);
         trx.clear();

         const asset_object &bitusd = get_asset("USDBIT");
         core = core_id(db);

         // The force MCFR should not be set
         BOOST_CHECK(!bitusd.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());


         ///////
         // 4. Asset owner fails to update the smart coin with a MCFR
         ///////
         const uint16_t mcfr_3 = 30; // 3% MCFR (BSIP74)
         asset_update_bitasset_operation uop;
         uop.issuer = assetowner_id;
         uop.asset_to_update = bitusd.get_id();
         uop.new_options = bitusd.bitasset_data(db).options;
         uop.new_options.extensions.value.margin_call_fee_ratio = mcfr_3;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, assetowner_private_key);
         REQUIRE_EXCEPTION_WITH_TEXT(PUSH_TX(db, trx), "cannot be set before Hardfork BSIP74");

         // The MCFR should not be set
         BOOST_CHECK(!bitusd.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());


         ///////
         // 5. Asset owner fails to update the smart coin with a MCFR in a proposal
         ///////
         {
            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(uop);

            trx.clear();
            trx.operations.push_back(cop);
            // sign(trx, assetowner_private_key);
            REQUIRE_EXCEPTION_WITH_TEXT(PUSH_TX(db, trx), "cannot be set before Hardfork BSIP74");

            // The MCFR should not be set
            BOOST_CHECK(!bitusd.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());
         }


         ///////
         // 6. Activate HARDFORK_CORE_BSIP74_TIME
         ///////
         BOOST_CHECK(db.head_block_time() < HARDFORK_CORE_BSIP74_TIME); // Confirm still before hardfork activation
         BOOST_TEST_MESSAGE("Advancing past Hardfork BSIP74");
         generate_blocks(HARDFORK_CORE_BSIP74_TIME);
         generate_block();
         set_expiration(db, trx);
         trx.clear();


         ///////
         // 7. Asset owner succeeds to create the smart coin called CNYBIT with a MCFR
         ///////
         {
            mcfr_opt = mcfr_3;
            const asset_create_operation create_op = make_bitasset("CNYBIT", assetowner_id, market_fee_percent,
                                                                   charge_market_fee, 4, core_id,
                                                                   GRAPHENE_MAX_SHARE_SUPPLY, icr_opt, mcfr_opt);

            trx.clear();
            trx.operations.push_back(create_op);
            sign(trx, assetowner_private_key);
            PUSH_TX(db, trx); // No exception should be thrown
         }

         generate_block();
         set_expiration(db, trx);
         trx.clear();

         const auto &bitcny = get_asset("CNYBIT");

         // The MCFR should be set
         BOOST_CHECK(bitcny.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());
         BOOST_CHECK_EQUAL(*bitcny.bitasset_data(db).options.extensions.value.margin_call_fee_ratio, mcfr_3);


         ///////
         // 8. Asset owner succeeds to create the smart coin called RUBBIT with a MCFR in a proposal
         ///////
         const uint16_t mcfr_1 = 10; // 1% expressed in terms of GRAPHENE_COLLATERAL_RATIO_DENOM
         {
            // Create the proposal
            mcfr_opt = mcfr_1;
            const asset_create_operation create_op = make_bitasset("RUBBIT", assetowner_id, market_fee_percent,
                                                                   charge_market_fee, 4, core_id,
                                                                   GRAPHENE_MAX_SHARE_SUPPLY, icr_opt, mcfr_opt);

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(create_op);

            trx.clear();
            trx.operations.push_back(cop);
            // sign(trx, assetowner_private_key);
            processed_transaction processed = PUSH_TX(db, trx);


            // Approve the proposal
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = assetowner_id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(assetowner_id);
            trx.clear();
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, assetowner_private_key);

            PUSH_TX(db, trx); // No exception should be thrown


            // Advance to the activation of the proposal
            generate_blocks(cop.expiration_time);
            set_expiration(db, trx);
         }
         const auto &bitrub = get_asset("RUBBIT");

         // The MCFR should be set
         BOOST_CHECK(bitrub.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());
         BOOST_CHECK_EQUAL(*bitrub.bitasset_data(db).options.extensions.value.margin_call_fee_ratio, mcfr_1);


         ///////
         // 9. Asset owner succeeds to update the smart coin called CNYBIT with a MCFR
         ///////
         uop = asset_update_bitasset_operation();
         uop.issuer = assetowner_id;
         uop.asset_to_update = bitcny.get_id();
         uop.new_options = bitcny.bitasset_data(db).options;
         uop.new_options.extensions.value.margin_call_fee_ratio = mcfr_5;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, assetowner_private_key);
         PUSH_TX(db, trx);

         // The MCFR should be set
         BOOST_CHECK(bitcny.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());
         BOOST_CHECK_EQUAL(*bitcny.bitasset_data(db).options.extensions.value.margin_call_fee_ratio, mcfr_5);


         ///////
         // 10. Asset owner succeeds to update the smart coin called RUBBIT with a MCFR in a proposal
         ///////
         {
            // Create the proposal
            uop = asset_update_bitasset_operation();
            uop.issuer = assetowner_id;
            uop.asset_to_update = bitrub.get_id();
            uop.new_options = bitrub.bitasset_data(db).options;
            uop.new_options.extensions.value.margin_call_fee_ratio = mcfr_5;

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(uop);

            trx.clear();
            trx.operations.push_back(cop);
            // sign(trx, assetowner_private_key);
            processed_transaction processed = PUSH_TX(db, trx);


            // Approve the proposal
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = assetowner_id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(assetowner_id);
            trx.clear();
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, assetowner_private_key);

            PUSH_TX(db, trx); // No exception should be thrown

            // Advance to the activation of the proposal
            generate_blocks(cop.expiration_time);
            set_expiration(db, trx);
         }

         // The MCFR should be set
         BOOST_CHECK(bitrub.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());
         BOOST_CHECK_EQUAL(*bitrub.bitasset_data(db).options.extensions.value.margin_call_fee_ratio, mcfr_5);


         ///////
         // 11. Asset owner succeeds to create the smart coin called YENBIT without a MCFR
         ///////
         {
            const asset_create_operation create_op = make_bitasset("YENBIT", assetowner_id, market_fee_percent,
                                                                   charge_market_fee, 4, core_id,
                                                                   GRAPHENE_MAX_SHARE_SUPPLY, icr_opt, mcfr_null_opt);

            trx.clear();
            trx.operations.push_back(create_op);
            sign(trx, assetowner_private_key);
            PUSH_TX(db, trx); // No exception should be thrown
         }

         generate_block();
         set_expiration(db, trx);
         trx.clear();

         const auto &bityen = get_asset("YENBIT");

         // The MCFR should be set
         BOOST_CHECK(!bityen.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());


         ///////
         // 12. Asset owner succeeds to update the smart coin called RUBBIT without a MCFR in a proposal
         ///////
         {
            // Create the proposal
            uop = asset_update_bitasset_operation();
            uop.issuer = assetowner_id;
            uop.asset_to_update = bitrub.get_id();
            uop.new_options = bitrub.bitasset_data(db).options;
            uop.new_options.extensions.value.margin_call_fee_ratio = mcfr_null_opt;

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(uop);

            trx.clear();
            trx.operations.push_back(cop);
            // sign(trx, assetowner_private_key);
            processed_transaction processed = PUSH_TX(db, trx);


            // Approve the proposal
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = assetowner_id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(assetowner_id);
            trx.clear();
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, assetowner_private_key);

            PUSH_TX(db, trx); // No exception should be thrown

            // Advance to the activation of the proposal
            generate_blocks(cop.expiration_time);
            set_expiration(db, trx);
         }

         // The MCFR should NOT be set
         BOOST_CHECK(!bitrub.bitasset_data(db).options.extensions.value.margin_call_fee_ratio.valid());


      } FC_LOG_AND_RETHROW()
   }


BOOST_AUTO_TEST_SUITE_END()
