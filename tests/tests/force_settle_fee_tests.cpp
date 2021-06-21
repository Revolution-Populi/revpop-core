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

#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/market_object.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

struct force_settle_database_fixture : database_fixture {
   force_settle_database_fixture()
           : database_fixture() {
   }

   /**
    * Create an operation to create a smart asset
    * @param name Asset name
    * @param issuer Issuer ID
    * @param force_settlement_offset_percent Force-settlement offset percent
    * @param force_settlement_fee_percent Force-settlement fee percent (BSIP87)
    * @return An asset_create_operation
    */
   const asset_create_operation create_smart_asset_op(
           const string &name,
           account_id_type issuer /* = GRAPHENE_WITNESS_ACCOUNT */,
           uint16_t force_settlement_offset_percent /* 100 = 1% */,
           optional<uint16_t> force_settlement_fee_percent /* 100 = 1% */
   ) {
      try {
         uint16_t market_fee_percent = 100; /*1%*/
         uint16_t flags = charge_market_fee;
         uint16_t precision = 2;
         asset_id_type backing_asset = {};
         share_type max_supply = GRAPHENE_MAX_SHARE_SUPPLY;

         asset_create_operation creator;
         creator.issuer = issuer;
         creator.fee = asset();
         creator.symbol = name;
         creator.precision = precision;

         creator.common_options.max_supply = max_supply;
         creator.common_options.market_fee_percent = market_fee_percent;
         if (issuer == GRAPHENE_WITNESS_ACCOUNT)
            flags |= witness_fed_asset;
         creator.common_options.issuer_permissions = flags;
         creator.common_options.flags = flags & ~global_settle;
         creator.common_options.core_exchange_rate = price(asset(1, asset_id_type(1)), asset(1));

         creator.bitasset_opts = bitasset_options();
         creator.bitasset_opts->force_settlement_offset_percent = force_settlement_offset_percent;
         creator.bitasset_opts->short_backing_asset = backing_asset;
         creator.bitasset_opts->extensions.value.force_settle_fee_percent = force_settlement_fee_percent;

         return creator;

      } FC_CAPTURE_AND_RETHROW((name)(issuer))
   }


   /**
    * Create a smart asset without a force settlement fee percent
    * @param name Asset name
    * @param issuer Issuer ID
    * @param force_settlement_offset_percent Force-settlement offset percent
    * @return Asset object
    */
   const asset_object &create_smart_asset(
           const string &name,
           account_id_type issuer /* = GRAPHENE_WITNESS_ACCOUNT */,
           uint16_t force_settlement_offset_percent /* = 100 */ /* 1% */
   ) {
      try {
         optional<uint16_t> force_settlement_fee_percent; // Not specified
         return create_smart_asset(name, issuer, force_settlement_offset_percent, force_settlement_fee_percent);
      } FC_CAPTURE_AND_RETHROW((name)(issuer)(force_settlement_offset_percent))
   }


   /**
    * Create a smart asset
    * @param name Asset name
    * @param issuer Issuer ID
    * @param force_settlement_offset_percent Force-settlement offset percent
    * @param force_settlement_fee_percent Force-settlement fee percent (BSIP87)
    * @return Asset object
    */
   const asset_object &create_smart_asset(
           const string &name,
           account_id_type issuer /* = GRAPHENE_WITNESS_ACCOUNT */,
           uint16_t force_settlement_offset_percent /* 100 = 1% */,
           optional<uint16_t> force_settlement_fee_percent /* 100 = 1% */
   ) {
      try {
         asset_create_operation creator = create_smart_asset_op(name, issuer, force_settlement_offset_percent,
                                                                force_settlement_fee_percent);

         trx.operations.push_back(std::move(creator));
         trx.validate();
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.operations.clear();
         return db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      } FC_CAPTURE_AND_RETHROW((name)(issuer))
   }
};


/**
 * Test the effects of the new force settlement fee from BSIP87
 */
BOOST_FIXTURE_TEST_SUITE(force_settle_tests, force_settle_database_fixture)

   /**
    * Test the ability to create and update assets with force-settlement fee % before HARDFORK_CORE_BSIP87_TIME
    *
    *
    * Before HARDFORK_CORE_BSIP87_TIME
    *
    * 1. Asset owner fails to create the smart coin called USDBIT with a force-settlement fee %
    * 2. Asset owner fails to create the smart coin called USDBIT with a force-settlement fee % in a proposal
    * 3. Asset owner succeeds to create the smart coin called USDBIT without a force-settlement fee %
    *
    * 4. Asset owner fails to update the smart coin with a force-settlement fee %
    * 5. Asset owner fails to update the smart coin with a force-settlement fee % in a proposal
    *
    * 6. Asset owner fails to claim collateral-denominated fees
    * 7. Asset owner fails to claim collateral-denominated fees in a proposal
    *
    *
    * 8. Activate HARDFORK_CORE_BSIP87_TIME
    *
    *
    * After HARDFORK_CORE_BSIP87_TIME
    *
    * 9. Asset owner succeeds to create the smart coin called CNYBIT with a force-settlement fee %
    * 10. Asset owner succeeds to create the smart coin called RUBBIT with a force-settlement fee % in a proposal
    *
    * 11. Asset owner succeeds to update the smart coin called CNYBIT with a force-settlement fee %
    * 12. Asset owner succeeds to update the smart coin called RUBBIT with a force-settlement fee % in a proposal
    */
   BOOST_AUTO_TEST_CASE(prevention_before_hardfork_test) {
      try {
         ///////
         // Initialize the scenario
         ///////
         // Get around Graphene issue #615 feed expiration bug
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
         trx.clear();
         set_expiration(db, trx);

         // Create actors
         ACTORS((assetowner));

         // Fund actors
         uint64_t initial_balance_core = 10000000;
         transfer(committee_account, assetowner.id, asset(initial_balance_core));


         ///////
         // 1. Asset owner fails to create the smart coin called bitUSD with a force-settlement fee %
         ///////
         const uint16_t usd_fso_percent = 5 * GRAPHENE_1_PERCENT; // 5% Force-settlement offset fee %

         ///////
         // 2. Asset owner fails to create the smart coin called bitUSD with a force-settlement fee % in a proposal
         ///////


         ///////
         // 3. Asset owner succeeds to create the smart coin called bitUSD without a force-settlement fee %
         ///////
         trx.clear();
         create_smart_asset("USDBIT", assetowner.id, usd_fso_percent);


         ///////
         // 4. Asset owner fails to update the smart coin with a force-settlement fee %
         ///////
         asset_update_bitasset_operation uop;


         ///////
         // 5. Asset owner fails to update the smart coin with a force-settlement fee % in a proposal
         ///////



         ///////
         // 6. Asset owner fails to claim collateral-denominated fees
         ///////
         // Although no collateral-denominated fees should be present, the error should indicate the
         // that claiming such fees are not yet active.
         asset_claim_fees_operation claim_op;


         ///////
         // 7. Asset owner fails to claim collateral-denominated fees in a proposal
         ///////
         proposal_create_operation cop;


         ///////
         // 8. Activate HARDFORK_CORE_BSIP87_TIME
         ///////
         generate_block();
         set_expiration(db, trx);
         trx.clear();


         ///////
         // 9. Asset owner succeeds to create the smart coin called CNYBIT with a force-settlement fee %
         ///////
         const uint16_t fsf_percent_1 = 1 * GRAPHENE_1_PERCENT; // 1% Force-settlement fee % (BSIP87)
         const uint16_t fsf_percent_5 = 1 * GRAPHENE_1_PERCENT; // 5% Force-settlement fee % (BSIP87)
         trx.clear();
         create_smart_asset("CNYBIT", assetowner.id, usd_fso_percent, fsf_percent_1);

         generate_block();
         set_expiration(db, trx);
         trx.clear();

         const auto &bitcny = get_asset("CNYBIT");

         // The force settlement fee % should be set
         BOOST_CHECK(bitcny.bitasset_data(db).options.extensions.value.force_settle_fee_percent.valid());
         BOOST_CHECK_EQUAL(fsf_percent_1, *bitcny.bitasset_data(db).options.extensions.value.force_settle_fee_percent);


         ///////
         // 10. Asset owner succeeds to create the smart coin called RUBBIT with a force-settlement fee % in a proposal
         ///////
         {
            // Create the proposal
            asset_create_operation create_op = create_smart_asset_op("RUBBIT", assetowner.id, usd_fso_percent,
                                                                     fsf_percent_1);
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

         // The force settlement fee % should be set
         BOOST_CHECK(bitrub.bitasset_data(db).options.extensions.value.force_settle_fee_percent.valid());
         BOOST_CHECK_EQUAL(fsf_percent_1, *bitrub.bitasset_data(db).options.extensions.value.force_settle_fee_percent);


         ///////
         // 11. Asset owner succeeds to update the smart coin called CNYBIT with a force-settlement fee %
         ///////
         uop = asset_update_bitasset_operation();
         uop.issuer = assetowner.id;
         uop.asset_to_update = bitcny.get_id();
         uop.new_options = bitcny.bitasset_data(db).options;
         uop.new_options.extensions.value.force_settle_fee_percent = fsf_percent_5;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, assetowner_private_key);
         PUSH_TX(db, trx);

         // The force settlement fee % should be set
         BOOST_CHECK(bitcny.bitasset_data(db).options.extensions.value.force_settle_fee_percent.valid());
         BOOST_CHECK_EQUAL(fsf_percent_5, *bitcny.bitasset_data(db).options.extensions.value.force_settle_fee_percent);


         ///////
         // 12. Asset owner succeeds to update the smart coin called RUBBIT with a force-settlement fee % in a proposal
         ///////
         {
            // Create the proposal
            uop = asset_update_bitasset_operation();
            uop.issuer = assetowner.id;
            uop.asset_to_update = bitrub.get_id();
            uop.new_options = bitrub.bitasset_data(db).options;
            uop.new_options.extensions.value.force_settle_fee_percent = fsf_percent_5;

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

         // The force settlement fee % should be set
         BOOST_CHECK(bitrub.bitasset_data(db).options.extensions.value.force_settle_fee_percent.valid());
         BOOST_CHECK_EQUAL(fsf_percent_5, *bitrub.bitasset_data(db).options.extensions.value.force_settle_fee_percent);

      }
      FC_LOG_AND_RETHROW()
   }

BOOST_AUTO_TEST_SUITE_END()
