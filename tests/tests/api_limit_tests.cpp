/*
 * Copyright (c) 2018 contributors.
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

#include <boost/test/unit_test.hpp>

#include <graphene/app/database_api.hpp>
#include <graphene/chain/hardfork.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(api_limit_tests, database_fixture)

BOOST_AUTO_TEST_CASE( api_limit_get_key_references ){
   try{
   const int num_keys = 210;
   const int num_keys1 = 2;
   vector< private_key_type > numbered_private_keys;
   vector< public_key_type >  numbered_key_id;
   numbered_private_keys.reserve( num_keys );

   graphene::app::application_options opt1 = app.get_options();
   opt1.has_api_helper_indexes_plugin = false;
   graphene::app::database_api db_api1( db, &opt1 );
   BOOST_CHECK_THROW( db_api1.get_key_references(numbered_key_id), fc::exception );

   graphene::app::application_options opt = app.get_options();
   opt.has_api_helper_indexes_plugin = true;
   graphene::app::database_api db_api( db, &opt );

   for( int i=0; i<num_keys1; i++ )
   {
      private_key_type privkey = generate_private_key(std::string("key_") + std::to_string(i));
      public_key_type pubkey = privkey.get_public_key();
      numbered_private_keys.push_back( privkey );
      numbered_key_id.push_back( pubkey );
   }
   vector< flat_set<account_id_type> > final_result=db_api.get_key_references(numbered_key_id);
   BOOST_REQUIRE_EQUAL( final_result.size(), 2u );
   numbered_private_keys.reserve( num_keys );
   for( int i=num_keys1; i<num_keys; i++ )
   {
       private_key_type privkey = generate_private_key(std::string("key_") + std::to_string(i));
       public_key_type pubkey = privkey.get_public_key();
       numbered_private_keys.push_back( privkey );
       numbered_key_id.push_back( pubkey );
   }
   GRAPHENE_CHECK_THROW(db_api.get_key_references(numbered_key_id), fc::exception);
   }catch (fc::exception& e) {
   edump((e.to_detail_string()));
   throw;
   }
}

BOOST_AUTO_TEST_CASE( api_limit_get_full_accounts ) {

   try {
      ACTOR(alice);

      graphene::app::application_options opt = app.get_options();
      opt.has_api_helper_indexes_plugin = true;
      graphene::app::database_api db_api( db, &opt );

      vector<string> accounts;

      for( size_t i = 0; i < 50; ++i )
      {
         string account_name = "testaccount" + fc::to_string(i);
         create_account( account_name );
         accounts.push_back( account_name );
      }
      accounts.push_back( "alice" );

      transfer_operation op;
      op.from = alice_id;
      op.amount = asset(1);
      for( size_t i = 0; i < 501; ++i )
      {
         propose( op, alice_id );
      }

      // Too many accounts
      GRAPHENE_CHECK_THROW(db_api.get_full_accounts(accounts, false), fc::exception);

      accounts.erase(accounts.begin());
      auto full_accounts = db_api.get_full_accounts(accounts, false);
      BOOST_CHECK(full_accounts.size() == 50);

      // The default max list size is 500
      BOOST_REQUIRE( full_accounts.find("alice") != full_accounts.end() );
      BOOST_CHECK_EQUAL( full_accounts["alice"].proposals.size(), 500u );
      BOOST_CHECK( full_accounts["alice"].more_data_available.proposals );
      BOOST_REQUIRE( full_accounts.find("testaccount9") != full_accounts.end() );
      BOOST_CHECK_EQUAL( full_accounts["testaccount9"].proposals.size(), 0 );
      BOOST_CHECK( !full_accounts["testaccount9"].more_data_available.proposals );

      // not an account
      accounts.erase(accounts.begin());
      accounts.push_back("nosuchaccount");

      // non existing accounts will be ignored in the results
      full_accounts = db_api.get_full_accounts(accounts, false);
      BOOST_CHECK(full_accounts.size() == 49);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( api_limit_lookup_accounts ) {
   try{
      graphene::app::database_api db_api( db, &( app.get_options() ));
      ACTOR(bob);
      GRAPHENE_CHECK_THROW(db_api.lookup_accounts("bob",220), fc::exception);
      map<string,account_id_type> result =db_api.lookup_accounts("bob",190);
      BOOST_REQUIRE_EQUAL( result.size(), 17u);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( api_limit_lookup_witness_accounts ) {
   try{
      graphene::app::database_api db_api( db, &( app.get_options() ));
      ACTORS((bob)) ;
      GRAPHENE_CHECK_THROW(db_api.lookup_witness_accounts("bob",220), fc::exception);
      map<string, witness_id_type> result =db_api.lookup_witness_accounts("bob",190);
      BOOST_REQUIRE_EQUAL( result.size(), 10u);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE( api_limit_get_full_accounts2 ) {

   try {
      ACTOR(alice);

      graphene::app::application_options opt = app.get_options();
      opt.has_api_helper_indexes_plugin = true;
      graphene::app::database_api db_api( db, &opt );

      vector<string> accounts;
      for (int i=0; i<200; i++) {
         std::string acct_name = "mytempacct" + std::to_string(i);
         const account_object& account_name=create_account(acct_name);
         accounts.push_back(account_name.name);
      }
      accounts.push_back( "alice" );

      transfer_operation op;
      op.from = alice_id;
      op.amount = asset(1);
      for( size_t i = 0; i < 501; ++i )
      {
         propose( op, alice_id );
      }

      GRAPHENE_CHECK_THROW(db_api.get_full_accounts(accounts, false), fc::exception);
      accounts.erase(accounts.begin());
      auto full_accounts = db_api.get_full_accounts(accounts, false);
      BOOST_REQUIRE_EQUAL(full_accounts.size(), 200u);

      // The updated max list size is 120
      BOOST_REQUIRE( full_accounts.find("alice") != full_accounts.end() );
      BOOST_CHECK_EQUAL( full_accounts["alice"].proposals.size(), 120u );
      BOOST_CHECK( full_accounts["alice"].more_data_available.proposals );
      BOOST_REQUIRE( full_accounts.find("mytempacct9") != full_accounts.end() );
      BOOST_CHECK_EQUAL( full_accounts["mytempacct9"].proposals.size(), 0 );
      BOOST_CHECK( !full_accounts["mytempacct9"].more_data_available.proposals );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE(api_limit_get_withdraw_permissions_by_recipient){
   try{
      graphene::app::database_api db_api( db, &app.get_options());
      ACTORS((bob)) ;
      withdraw_permission_id_type withdraw_permission;
      GRAPHENE_CHECK_THROW(db_api.get_withdraw_permissions_by_recipient(
         "bob",withdraw_permission, 251), fc::exception);
      vector<withdraw_permission_object> result =db_api.get_withdraw_permissions_by_recipient(
         "bob",withdraw_permission,250);
      BOOST_REQUIRE_EQUAL( result.size(), 0u);
   }catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE(api_limit_get_withdraw_permissions_by_giver){
   try{
      graphene::app::database_api db_api( db, &app.get_options());
      ACTORS((bob)) ;
      withdraw_permission_id_type withdraw_permission;
      GRAPHENE_CHECK_THROW(db_api.get_withdraw_permissions_by_giver(
         "bob",withdraw_permission, 251), fc::exception);
      vector<withdraw_permission_object> result =db_api.get_withdraw_permissions_by_giver(
         "bob",withdraw_permission,250);
      BOOST_REQUIRE_EQUAL( result.size(), 0u);
   }catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE( api_limit_lookup_vote_ids ) {
   try{
      graphene::app::database_api db_api( db, &( app.get_options() ));
      ACTORS( (connie)(whitney)(wolverine) );
      fund(connie);
      upgrade_to_lifetime_member(connie);
      fund(whitney);
      upgrade_to_lifetime_member(whitney);
      fund(wolverine);
      upgrade_to_lifetime_member(wolverine);
      const auto& committee = create_committee_member( connie );
      const auto& witness = create_witness( whitney );
      const auto& worker = create_worker( wolverine_id );
      std::vector<vote_id_type> votes;
      votes.push_back( committee.vote_id );
      votes.push_back( witness.vote_id );
      const auto results = db_api.lookup_vote_ids( votes );
      BOOST_REQUIRE_EQUAL( results.size(), 2u);
      votes.push_back( worker.vote_for );
      GRAPHENE_CHECK_THROW(db_api.lookup_vote_ids(votes), fc::exception);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( api_limit_lookup_committee_member_accounts ) {
   try{
      graphene::app::database_api db_api( db, &( app.get_options() ));
      ACTORS((bob));
      GRAPHENE_CHECK_THROW(db_api.lookup_committee_member_accounts("bob",220), fc::exception);
      std::map<std::string, committee_member_id_type>  result =db_api.lookup_committee_member_accounts("bob",190);
      BOOST_REQUIRE_EQUAL( result.size(), 10u);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
