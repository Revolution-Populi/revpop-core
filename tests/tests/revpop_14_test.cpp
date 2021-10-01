/**
 * The Revolution Populi Project
 * Copyright (C) 2021 Revolution Populi Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <boost/test/unit_test.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( revpop_14_tests, database_fixture )

BOOST_AUTO_TEST_CASE( hardfork_time_test )
{ try {

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
         // Initialize committee by voting for each memeber and for desired count
         vote_for_committee_and_witnesses(INITIAL_COMMITTEE_MEMBER_COUNT, INITIAL_WITNESS_COUNT);
         generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
         set_expiration(db, trx);

      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      // Try to set new electoral threshold after hardfork
      proposal_create_operation cop = proposal_create_operation::committee_proposal(
            db.get_global_properties().parameters, db.head_block_time() );
      cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + 10;
      committee_member_update_global_parameters_operation cmuop;
      cmuop.new_parameters.extensions.value.electoral_threshold = 25;
      cop.proposed_ops.emplace_back(cmuop);
      trx.operations.push_back(cop);

      // Should fail since the value is too big
      GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      trx.operations.clear();
      cop.proposed_ops.clear();

      const chain_parameters& current_params = db.get_global_properties().parameters;
      chain_parameters new_params = current_params;
      new_params.extensions.value.electoral_threshold = 3; // 3 from 21
      cmuop.new_parameters = new_params;

      cop.proposed_ops.emplace_back(cmuop);
      trx.operations.push_back(cop);

      // Should succeed
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.operations.clear();
      proposal_id_type prop_id = ptx.operation_results[0].get<object_id_type>();

      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      // Approve the proposal
      proposal_update_operation uop;
      uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      uop.active_approvals_to_add = { get_account("init0").get_id(), get_account("init1").get_id(),
                                      get_account("init2").get_id(), get_account("init3").get_id(),
                                      get_account("init4").get_id(), get_account("init5").get_id(),
                                      get_account("init6").get_id(), get_account("init7").get_id() };
      trx.operations.push_back(uop);
      PUSH_TX(db, trx, ~0);

      // The electoral threshold is still 0
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 0 );

      generate_blocks( prop_id( db ).expiration_time + 5 );
      generate_blocks( db.get_dynamic_global_properties().next_maintenance_time );
      generate_block();

      // The maker fee discount percent should have changed
      BOOST_CHECK_EQUAL( db.get_global_properties().parameters.get_electoral_threshold(), 3 );

   }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

