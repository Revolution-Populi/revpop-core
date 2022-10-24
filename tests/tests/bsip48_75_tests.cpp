/*
 * Copyright (c) 2020 Abit More, and contributors.
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
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <boost/test/unit_test.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( bsip48_75_tests, database_fixture )

BOOST_AUTO_TEST_CASE( update_max_supply )
{
   try {

      generate_block();
      set_expiration( db, trx );

      ACTORS((nathan));

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", nathan, charge_market_fee );
      asset_id_type uia_id = uia.id;

      // issue some to Nathan
      issue_uia( nathan_id, uia.amount( GRAPHENE_MAX_SHARE_SUPPLY - 100 ) );

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update max supply to a smaller number
      asset_update_operation auop;
      auop.issuer = nathan_id;
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;
      auop.new_options.max_supply -= 90;

      trx.operations.clear();
      trx.operations.push_back( auop );

      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 90 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      generate_block();
      set_expiration( db, trx );

      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, uia_id(db).options.max_supply.value - 10 );
      BOOST_CHECK( uia_id(db).can_update_max_supply() );

      // able to set max supply to be equal to current supply
      auop.new_options.max_supply -= 10;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply == current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // no longer able to set max supply to a number smaller than current supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply == current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // increase max supply again
      auop.new_options.max_supply += 2;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // decrease max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update flag to disable updating of max supply
      auop.new_options.flags |= lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update flag to enable updating of max supply
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // able to update max supply
      auop.new_options.max_supply += 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update flag to disable updating of max supply
      auop.new_options.flags |= lock_max_supply;
      // update permission to disable updating of lock_max_supply flag
      auop.new_options.issuer_permissions |= lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // Able to propose the operation
      propose( auop );

      // unable to reinstall the permission
      auop.new_options.issuer_permissions &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.issuer_permissions |= lock_max_supply;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // unable to enable the lock_max_supply flag
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.flags |= lock_max_supply;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // able to update other parameters
      auto old_market_fee_percent = auop.new_options.market_fee_percent;
      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, old_market_fee_percent );

      auop.new_options.market_fee_percent = 120u;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, 120u );

      // reserve all supply
      reserve_asset( nathan_id, uia_id(db).amount( GRAPHENE_MAX_SHARE_SUPPLY - 100 ) );

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // still unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // still unable to enable the lock_max_supply flag
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.flags |= lock_max_supply;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // able to reinstall the permission and do it
      auop.new_options.issuer_permissions &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // still unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // now able to enable the lock_max_supply flag
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // issue some
      issue_uia( nathan_id, uia_id(db).amount( 100 ) );

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // update permission to disable updating of lock_max_supply flag
      auop.new_options.issuer_permissions |= lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      // still can update max supply
      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // unable to reinstall the permission
      auop.new_options.issuer_permissions &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.issuer_permissions |= lock_max_supply;

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( disable_new_supply_uia )
{
   try {

      set_expiration( db, trx );

      ACTORS((nathan));

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", nathan, charge_market_fee );
      asset_id_type uia_id = uia.id;

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // issue some to Nathan
      issue_uia( nathan_id, uia_id(db).amount( 100 ) );

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // prepare to update
      asset_update_operation auop;
      auop.issuer = nathan_id;
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;

      // update flag to disable creation of new supply
      auop.new_options.flags |= disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // unable to issue more coins
      BOOST_CHECK_THROW( issue_uia( nathan_id, uia_id(db).amount( 100 ) ), fc::exception );

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // update flag to enable creation of new supply
      auop.new_options.flags &= ~disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // issue some to Nathan
      issue_uia( nathan_id, uia_id(db).amount( 100 ) );

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // update flag to disable creation of new supply
      auop.new_options.flags |= disable_new_supply;
      // update permission to disable updating of disable_new_supply flag
      auop.new_options.issuer_permissions |= disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // Able to propose the operation
      propose( auop );

      // unable to reinstall the permission
      auop.new_options.issuer_permissions &= ~disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.issuer_permissions |= disable_new_supply;

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // unable to issue more coins
      BOOST_CHECK_THROW( issue_uia( nathan_id, uia_id(db).amount( 100 ) ), fc::exception );

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // unable to enable the disable_new_supply flag
      auop.new_options.flags &= ~disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.flags |= disable_new_supply;

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( skip_core_exchange_rate )
{
   try {

      set_expiration( db, trx );

      ACTORS((nathan));

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", nathan, charge_market_fee );
      asset_id_type uia_id = uia.id;

      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(1, uia_id), asset(1)) );

      // prepare to update
      asset_update_operation auop;
      auop.issuer = nathan_id;
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;

      // update CER
      auop.new_options.core_exchange_rate = price(asset(2, uia_id), asset(1));
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      // CER changed
      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(2, uia_id), asset(1)) );

      // save value for later check
      auto old_market_fee_percent = auop.new_options.market_fee_percent;
      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, old_market_fee_percent );

      // set skip_core_exchange_rate to false, should fail
      auop.new_options.core_exchange_rate = price(asset(3, uia_id), asset(1));
      auop.extensions.value.skip_core_exchange_rate = false;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      // unable to propose either
      BOOST_CHECK_THROW( propose( auop ), fc::exception );

      // CER didn't change
      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(2, uia_id), asset(1)) );

      // skip updating CER
      auop.extensions.value.skip_core_exchange_rate = true;
      auop.new_options.market_fee_percent = 120u;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      // CER didn't change
      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(2, uia_id), asset(1)) );
      // market_fee_percent changed
      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, 120u );

      // Able to propose the operation
      propose( auop );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( invalid_flags_in_asset )
{
   try {

      generate_block();
      set_expiration( db, trx );

      ACTORS((nathan)(feeder));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( nathan, asset(init_amount) );
      fund( feeder, asset(init_amount) );

      uint16_t bitmask = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      uint16_t uiamask = UIA_ASSET_ISSUER_PERMISSION_MASK & ~lock_max_supply & ~disable_new_supply; // Flag change permissions

      //uint16_t bitflag = ~global_settle & ~committee_fed_asset; // high bits are set
      //uint16_t uiaflag = ~(bitmask ^ uiamask); // high bits are set

      // Able to create UIA with invalid flags
      asset_create_operation acop;
      acop.issuer = nathan_id;
      acop.symbol = "NATHANCOIN";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100;
      //acop.common_options.flags = uiaflag;
      acop.common_options.issuer_permissions = uiamask;

      trx.operations.clear();
      trx.operations.push_back( acop );

      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& nathancoin = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type nathancoin_id = nathancoin.id;

      // There are invalid bits in flags
      BOOST_CHECK( !(nathancoin_id(db).options.flags & ~UIA_VALID_FLAGS_MASK) );

      // Able to create MPA with invalid flags
      asset_create_operation acop2 = acop;
      acop2.symbol = "NATHANBIT";
      acop2.bitasset_opts = bitasset_options();
      //acop2.common_options.flags = bitflag;
      acop2.common_options.issuer_permissions = bitmask;

      trx.operations.clear();
      trx.operations.push_back( acop2 );

      GRAPHENE_CHECK_THROW( PUSH_TX( db, trx, ~0 ), fc::assert_exception );

      // Unable to create a new UIA with an unknown bit in flags
      acop.symbol = "NEWNATHANCOIN";
      // With all possible bits in permissions set to 1
      acop2.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
      for( uint16_t bit = 0x8000; bit > 0; bit >>= 1 )
      {
         acop.common_options.flags = UIA_VALID_FLAGS_MASK | bit;
         if( acop.common_options.flags == UIA_VALID_FLAGS_MASK )
            continue;
         trx.operations.clear();
         trx.operations.push_back( acop );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         // Unable to propose either
         BOOST_CHECK_THROW( propose( acop ), fc::exception );
      }

      // Able to create a new UIA with a valid flags field
      acop.common_options.flags = UIA_VALID_FLAGS_MASK;
      trx.operations.clear();
      trx.operations.push_back( acop );
      ptx = PUSH_TX(db, trx, ~0);
      const asset_object& newnathancoin = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type newnathancoin_id = newnathancoin.id;

      BOOST_CHECK_EQUAL( newnathancoin_id(db).options.flags, UIA_VALID_FLAGS_MASK );

      // Able to propose too
      propose( acop );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_SUITE_END()

