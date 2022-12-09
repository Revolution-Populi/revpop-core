/*
 * Copyright (c) 2022 Revolution Populi Limited, and contributors.
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

#include <numeric>

#include <graphene/app/database_api.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <boost/test/unit_test.hpp>

#define BOOST_TEST_MODULE Update global properties tests

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

BOOST_FIXTURE_TEST_SUITE( revpop_14_tests, database_fixture )

BOOST_AUTO_TEST_CASE( hardfork_time_test )
{ try {

   // Initialize committee by voting for each memeber and for desired count
   vote_for_committee_and_witnesses(INITIAL_COMMITTEE_MEMBER_COUNT, INITIAL_WITNESS_COUNT);
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   set_expiration(db, trx);

   application_options opt = app.get_options();
   opt.has_api_helper_indexes_plugin = true;
   database_api db_api( db, &opt );

   {
      // The electoral threshold is 0 by default
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      // Try to set new committee parameter before hardfork
      proposal_create_operation cop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time() );
      cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + 10;
      committee_member_update_global_parameters_operation cmuop;
      cmuop.new_parameters.extensions.value.electoral_threshold = 1;
      cop.proposed_ops.emplace_back( cmuop );
      trx.operations.push_back( cop );

      // It should fail
      GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      trx.clear();

      // The electoral threshold still be 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );
   }

   // Pass the hardfork
   generate_blocks( HARDFORK_REVPOP_14_TIME );
   set_expiration( db, trx );

   {
      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );
      const chain_parameters& current_params = db.get_global_properties().parameters;
      chain_parameters new_params = current_params;

      // Try to set new electoral threshold after hardfork
      proposal_create_operation cop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time() );
      cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + 10;
      committee_member_update_global_parameters_operation cmuop;
      new_params.extensions.value.electoral_threshold = 25;
      cmuop.new_parameters = new_params;
      cop.proposed_ops.emplace_back(cmuop);
      trx.operations.push_back(cop);

      // Should fail since the value is too big
      GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      trx.operations.clear();
      cop.proposed_ops.clear();

      new_params.extensions.value.electoral_threshold = 3; // 3 from 21
      cmuop.new_parameters = new_params;

      cop.proposed_ops.emplace_back(cmuop);
      trx.operations.push_back(cop);

      // Check the fee
      vector< fc::variant > fees = db_api.get_required_fees(trx.operations, "RVP");
      BOOST_CHECK_EQUAL( fees.size(), 1 );
      std::pair< asset, fc::variants > result;
      from_variant( fees[0], result, GRAPHENE_NET_MAX_NESTED_OBJECTS );
      const auto& curfees = current_params.get_current_fees();
      BOOST_CHECK_EQUAL(result.first.amount.value, curfees.get<proposal_create_operation>().fee );

      // Should succeed
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.operations.clear();
      proposal_id_type prop_id = ptx.operation_results[0].get<object_id_type>();

      // One active proposal is pending approval
      BOOST_CHECK_EQUAL( db_api.get_proposed_global_parameters().size(), 1 );

      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      // Approve the proposal
      proposal_update_operation uop;
      uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      uop.proposal = prop_id;
      uop.active_approvals_to_add = { get_account("init0").get_id(), get_account("init1").get_id(),
                                      get_account("init2").get_id(), get_account("init3").get_id(),
                                      get_account("init4").get_id(), get_account("init5").get_id(),
                                      get_account("init6").get_id(), get_account("init7").get_id() };
      trx.operations.push_back(uop);
      PUSH_TX(db, trx, ~0);

      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );
      BOOST_CHECK_EQUAL( db_api.get_proposed_global_parameters().size(), 1 );

      // Make sure that the received operation really offers to change the network parameters
      vector<operation> operations = db_api
                                       .get_proposed_global_parameters()
                                       .at(0)
                                       .proposed_transaction.operations;
      BOOST_CHECK( find_if(begin(operations), end(operations), [](const op_wrapper &op)
                   {
                      return op.op.is_type<committee_member_update_global_parameters_operation>();
                   }) != std::end(operations) );

      generate_blocks( prop_id( db ).expiration_time + 5 );
      generate_blocks( db.get_dynamic_global_properties().next_maintenance_time );
      generate_block();

      // No active proposals
      BOOST_CHECK_EQUAL( db_api.get_proposed_global_parameters().size(), 0 );

      // The maker fee discount percent should have changed
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 3 );

   }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

