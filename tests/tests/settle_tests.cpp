/*
 * Copyright (c) 2018 oxarbitrage, and contributors.
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

BOOST_FIXTURE_TEST_SUITE( settle_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_bitassets )
{
   try {

      generate_block();
      set_expiration( db, trx );

      ACTORS((paul)(rachelregistrar)(rachelreferrer));

      upgrade_to_lifetime_member(rachelregistrar);
      upgrade_to_lifetime_member(rachelreferrer);

      constexpr auto market_fee_percent      = 50 * GRAPHENE_1_PERCENT;
      constexpr auto biteur_reward_percent   = 90 * GRAPHENE_1_PERCENT;
      constexpr auto referrer_reward_percent = 10 * GRAPHENE_1_PERCENT;

      const auto& biteur = create_bitasset( "EURBIT", paul_id, market_fee_percent, charge_market_fee, 2 );
      asset_id_type biteur_id = biteur.id;

      const auto& bitusd = create_bitasset( "USDBIT", paul_id, market_fee_percent, charge_market_fee, 2, biteur_id );

      const account_object rachel  = create_account( "rachel", rachelregistrar, rachelreferrer,
                                                     referrer_reward_percent );

      transfer( committee_account, rachelregistrar_id, asset( 10000000 ) );
      transfer( committee_account, rachelreferrer_id, asset( 10000000 ) );
      transfer( committee_account, rachel.get_id(), asset( 10000000) );
      transfer( committee_account, paul_id, asset( 10000000000 ) );

      asset_update_operation op;
      op.issuer = biteur.issuer;
      op.asset_to_update = biteur_id;
      op.new_options.issuer_permissions = charge_market_fee;
      op.new_options.extensions.value.reward_percent = biteur_reward_percent;
      op.new_options.flags = bitusd.options.flags | charge_market_fee;
      op.new_options.core_exchange_rate = price( asset(20,biteur_id), asset(1,asset_id_type()) );
      op.new_options.market_fee_percent = market_fee_percent;
      trx.operations.push_back(op);
      sign(trx, paul_private_key);
      PUSH_TX(db, trx);
      generate_block();
      trx.clear();
      set_expiration( db, trx );
   } FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_SUITE_END()
