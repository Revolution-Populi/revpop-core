/*
 * Copyright (c) 2020 contributors.
 * Copyright (c) 2020-2023 Revolution Populi Limited, and contributors.
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

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <boost/test/unit_test.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( bsip86_tests, database_fixture )

BOOST_AUTO_TEST_CASE( hardfork_time_test )
{ try {
   // Initialize committee by voting for each memeber and for desired count
   vote_for_committee_and_witnesses(INITIAL_COMMITTEE_MEMBER_COUNT, INITIAL_WITNESS_COUNT);
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   set_expiration(db, trx);

   // Pass the hardfork
   generate_block();
   set_expiration( db, trx );

   {
      // The network fee percent is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_market_fee_network_percent(), 0 );

      // Try to set new committee parameter after hardfork
      proposal_create_operation cop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time() );
      cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + 10;
      committee_member_update_global_parameters_operation cmuop;
      cmuop.new_parameters.extensions.value.market_fee_network_percent = 3001; // 30.01%
      cop.proposed_ops.emplace_back(cmuop);
      trx.operations.push_back(cop);

      // Should fail since the value is too big
      GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      // The network fee percent is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_market_fee_network_percent(), 0 );

      trx.operations.clear();
      cop.proposed_ops.clear();
      cmuop.new_parameters.extensions.value.market_fee_network_percent = 1123; // 11.23%
      cop.proposed_ops.emplace_back(cmuop);
      trx.operations.push_back(cop);

      // Should succeed
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.operations.clear();
      proposal_id_type prop_id = ptx.operation_results[0].get<object_id_type>();

      // The network fee percent is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_market_fee_network_percent(), 0 );

      // Approve the proposal
      proposal_update_operation uop;
      uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      uop.active_approvals_to_add = { get_account("init0").get_id(), get_account("init1").get_id(),
                                      get_account("init2").get_id(), get_account("init3").get_id(),
                                      get_account("init4").get_id(), get_account("init5").get_id(),
                                      get_account("init6").get_id(), get_account("init7").get_id() };
      trx.operations.push_back(uop);
      PUSH_TX(db, trx, ~0);

      // The network fee percent is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_market_fee_network_percent(), 0 );

      generate_blocks( prop_id( db ).expiration_time + 5 );
      generate_blocks( db.get_dynamic_global_properties().next_maintenance_time );
      generate_block();

      // The network fee percent should have changed
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_market_fee_network_percent(), 1123 );

   }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
