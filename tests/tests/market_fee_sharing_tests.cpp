#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/app/database_api.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>


#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace fc
{
   template<typename Ch, typename T>
   std::basic_ostream<Ch>& operator<<(std::basic_ostream<Ch>& os, safe<T> const& sf)
   {
      os << sf.value;
      return os;
   }
}

struct reward_database_fixture : database_fixture
{
   using whitelist_market_fee_sharing_t = fc::optional<flat_set<account_id_type>>;

   reward_database_fixture()
      : database_fixture()
   {
   }

   void update_asset( const account_id_type& issuer_id,
                      const fc::ecc::private_key& private_key,
                      const asset_id_type& asset_id,
                      uint16_t reward_percent,
                      const whitelist_market_fee_sharing_t &whitelist_market_fee_sharing = whitelist_market_fee_sharing_t{},
                      const flat_set<account_id_type> &blacklist = flat_set<account_id_type>())
   {
      asset_update_operation op;
      op.issuer = issuer_id;
      op.asset_to_update = asset_id;
      op.new_options = asset_id(db).options;
      op.new_options.extensions.value.reward_percent = reward_percent;
      op.new_options.extensions.value.whitelist_market_fee_sharing = whitelist_market_fee_sharing;
      op.new_options.blacklist_authorities = blacklist;

      signed_transaction tx;
      tx.operations.push_back( op );
      db.current_fee_schedule().set_fee( tx.operations.back() );
      set_expiration( db, tx );
      sign( tx, private_key );
      PUSH_TX( db, tx );
   }

   void asset_update_blacklist_authority(const account_id_type& issuer_id,
                                         const asset_id_type& asset_id,
                                         const account_id_type& authority_account_id,
                                         const fc::ecc::private_key& issuer_private_key)
   {
      asset_update_operation uop;
      uop.issuer = issuer_id;
      uop.asset_to_update = asset_id;
      uop.new_options = asset_id(db).options;
      uop.new_options.blacklist_authorities.insert(authority_account_id);

      signed_transaction tx;
      tx.operations.push_back( uop );
      db.current_fee_schedule().set_fee( tx.operations.back() );
      set_expiration( db, tx );
      sign( tx, issuer_private_key );
      PUSH_TX( db, tx );
   }

   void add_account_to_blacklist(const account_id_type& authorizing_account_id,
                                 const account_id_type& blacklisted_account_id,
                                 const fc::ecc::private_key& authorizing_account_private_key)
   {
      account_whitelist_operation wop;
      wop.authorizing_account = authorizing_account_id;
      wop.account_to_list = blacklisted_account_id;
      wop.new_listing = account_whitelist_operation::black_listed;

      signed_transaction tx;
      tx.operations.push_back( wop );
      db.current_fee_schedule().set_fee( tx.operations.back() );
      set_expiration( db, tx );
      sign( tx, authorizing_account_private_key );
      PUSH_TX( db, tx);
   }

   asset core_asset(int64_t x )
   {
       return asset( x*core_precision );
   };

   const share_type core_precision = asset::scaled_precision( asset_id_type()(db).precision );

   void create_vesting_balance_object(const account_id_type& account_id, vesting_balance_type balance_type )
   {
      db.create<vesting_balance_object>([&account_id, balance_type] (vesting_balance_object &vbo) {
         vbo.owner = account_id;
         vbo.balance_type = balance_type;
      });
   };
};

BOOST_FIXTURE_TEST_SUITE( fee_sharing_tests, reward_database_fixture )

BOOST_AUTO_TEST_CASE(create_asset_with_reward_percent_of_100_after_hf1774)
{
   try
   {
      generate_block();

      ACTOR(nathan);

      uint16_t reward_percent = GRAPHENE_100_PERCENT; // 100.00%
      flat_set<account_id_type> whitelist = {nathan_id};
      price price(asset(1, asset_id_type(1)), asset(1));
      uint16_t market_fee_percent = 100;

      additional_asset_options_t options;
      options.value.reward_percent = reward_percent;
      options.value.whitelist_market_fee_sharing = whitelist;

      asset_object usd_asset = create_user_issued_asset("USD",
                                                        nathan,
                                                        charge_market_fee,
                                                        price,
                                                        2,
                                                        market_fee_percent,
                                                        options);

      additional_asset_options usd_options = usd_asset.options.extensions.value;
      BOOST_CHECK_EQUAL(reward_percent, *usd_options.reward_percent);
      BOOST_CHECK(whitelist == *usd_options.whitelist_market_fee_sharing);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(set_reward_percent_to_100_after_hf1774)
{
   try
   {
      ACTOR(nathan);

      asset_object usd_asset = create_user_issued_asset("USD", nathan, charge_market_fee); // make a copy

      generate_block();

      uint16_t reward_percent = GRAPHENE_100_PERCENT; // 100.00%
      flat_set<account_id_type> whitelist = {nathan_id};
      update_asset(nathan_id, nathan_private_key, usd_asset.get_id(), reward_percent, whitelist);

      additional_asset_options options = usd_asset.get_id()(db).options.extensions.value;
      BOOST_CHECK_EQUAL(reward_percent, *options.reward_percent);
      BOOST_CHECK(whitelist == *options.whitelist_market_fee_sharing);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(create_actors)
{
   try
   {
      ACTORS((nathan)(izzyregistrar)(izzyreferrer)(tempregistrar));

      upgrade_to_lifetime_member(izzyregistrar);
      upgrade_to_lifetime_member(izzyreferrer);
      upgrade_to_lifetime_member(tempregistrar);

      price price(asset(1, asset_id_type(1)), asset(1));
      uint16_t market_fee_percent = 20 * GRAPHENE_1_PERCENT;
      const asset_object nathancoin = create_user_issued_asset( "JCOIN", nathan, charge_market_fee,
                                                              price, 2, market_fee_percent );

      const account_object alice = create_account("alice", izzyregistrar, izzyreferrer, 50/*0.5%*/);
      const account_object bob   = create_account("bob",   izzyregistrar, izzyreferrer, 50/*0.5%*/);
      const account_object old   = create_account("old",   GRAPHENE_TEMP_ACCOUNT(db),
                                                           GRAPHENE_COMMITTEE_ACCOUNT(db), 50u);
      const account_object tmp   = create_account("tmp",   tempregistrar,
                                                           GRAPHENE_TEMP_ACCOUNT(db), 50u);

      // prepare users' balance
      issue_uia( alice, nathancoin.amount( 20000000 ) );

      transfer( committee_account, alice.get_id(), core_asset(1000) );
      transfer( committee_account, bob.get_id(),   core_asset(1000) );
      transfer( committee_account, old.get_id(),   core_asset(1000) );
      transfer( committee_account, tmp.get_id(),   core_asset(1000) );
      transfer( committee_account, izzyregistrar.get_id(),  core_asset(1000) );
      transfer( committee_account, izzyreferrer.get_id(),  core_asset(1000) );
      transfer( committee_account, tempregistrar.get_id(),  core_asset(1000) );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(create_asset_via_proposal_test)
{
   try
   {
      ACTOR(issuer);
      price core_exchange_rate(asset(1, asset_id_type(1)), asset(1));

      asset_create_operation create_op;
      create_op.issuer = issuer.id;
      create_op.fee = asset();
      create_op.symbol = "ASSET";
      create_op.common_options.max_supply = 0;
      create_op.precision = 2;
      create_op.common_options.core_exchange_rate = core_exchange_rate;
      create_op.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      create_op.common_options.flags = charge_market_fee;

      additional_asset_options_t options;
      options.value.reward_percent = 100;
      options.value.whitelist_market_fee_sharing = flat_set<account_id_type>{issuer_id};
      create_op.common_options.extensions = std::move(options);;

      const auto& curfees = *db.get_global_properties().parameters.current_fees;
      const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
      proposal_create_operation prop;
      prop.fee_paying_account = issuer_id;
      prop.proposed_ops.emplace_back( create_op );
      prop.expiration_time =  db.head_block_time() + fc::days(1);
      prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

      {
         prop.expiration_time =  db.head_block_time() + fc::days(1);
         signed_transaction tx;
         tx.operations.push_back( prop );
         db.current_fee_schedule().set_fee( tx.operations.back() );
         set_expiration( db, tx );
         sign( tx, issuer_private_key );
         PUSH_TX( db, tx );
      }
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(update_asset_via_proposal_test)
{
   try
   {
      ACTOR(nathan);
      asset_object usd_asset = create_user_issued_asset("USD", nathan, charge_market_fee);

      additional_asset_options_t options;
      options.value.reward_percent = 100;
      options.value.whitelist_market_fee_sharing = flat_set<account_id_type>{nathan_id};

      asset_update_operation update_op;
      update_op.issuer = nathan_id;
      update_op.asset_to_update = usd_asset.get_id();
      asset_options new_options;
      update_op.new_options = usd_asset.options;
      update_op.new_options.extensions = std::move(options);

      const auto& curfees = *db.get_global_properties().parameters.current_fees;
      const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
      proposal_create_operation prop;
      prop.fee_paying_account = nathan_id;
      prop.proposed_ops.emplace_back( update_op );
      prop.expiration_time =  db.head_block_time() + fc::days(1);
      prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

      {
         prop.expiration_time =  db.head_block_time() + fc::days(1);
         signed_transaction tx;
         tx.operations.push_back( prop );
         db.current_fee_schedule().set_fee( tx.operations.back() );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(issue_asset){
   try
   {
       ACTORS((alice)(bob)(izzy)(nathan));
      // Izzy issues asset to Alice  (Izzycoin market percent - 10%)
      // Nathan issues asset to Bob    (Jillcoin market percent - 20%)

      fund( alice, core_asset(1000000) );
      fund( bob, core_asset(1000000) );
      fund( izzy, core_asset(1000000) );
      fund( nathan, core_asset(1000000) );

      price price(asset(1, asset_id_type(1)), asset(1));
      constexpr auto izzycoin_market_percent = 10*GRAPHENE_1_PERCENT;
      asset_object izzycoin = create_user_issued_asset( "IZZYCOIN", nathan,  charge_market_fee, price, 2, izzycoin_market_percent );

      constexpr auto nathancoin_market_percent = 20*GRAPHENE_1_PERCENT;
      asset_object nathancoin = create_user_issued_asset( "JILLCOIN", nathan,  charge_market_fee, price, 2, nathancoin_market_percent );

      // Alice and Bob create some coins
      issue_uia( alice, izzycoin.amount( 100000 ) );
      issue_uia( bob, nathancoin.amount( 100000 ) );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_vesting_balance_with_instant_vesting_policy_test )
{ try {

   ACTOR(alice);
   fund(alice);

   const asset_object& core = asset_id_type()(db);

   vesting_balance_create_operation op;
   op.fee = core.amount( 0 );
   op.creator = alice_id;
   op.owner = alice_id;
   op.amount = core.amount( 100 );
   op.policy = instant_vesting_policy_initializer{};

   trx.operations.push_back(op);
   set_expiration( db, trx );

   processed_transaction ptx = PUSH_TX( db, trx, ~0 );
   const vesting_balance_id_type& vbid = ptx.operation_results.back().get<object_id_type>();

   auto withdraw = [&](const asset& amount) {
      vesting_balance_withdraw_operation withdraw_op;
      withdraw_op.vesting_balance = vbid;
      withdraw_op.owner = alice_id;
      withdraw_op.amount = amount;

      signed_transaction withdraw_tx;
      withdraw_tx.operations.push_back( withdraw_op );
      set_expiration( db, withdraw_tx );
      sign(withdraw_tx, alice_private_key);
      PUSH_TX( db, withdraw_tx );
   };
   // try to withdraw more then it is on the balance
   GRAPHENE_REQUIRE_THROW(withdraw(op.amount.amount + 1), fc::exception);
   //to withdraw all that is on the balance
   withdraw(op.amount);
   // try to withdraw more then it is on the balance
   GRAPHENE_REQUIRE_THROW(withdraw( core.amount(1) ), fc::exception);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( create_vesting_balance_with_instant_vesting_policy_via_proposal_test )
{ try {

   ACTOR(actor);
   fund(actor);

   const asset_object& core = asset_id_type()(db);

   vesting_balance_create_operation create_op;
   create_op.fee = core.amount( 0 );
   create_op.creator = actor_id;
   create_op.owner = actor_id;
   create_op.amount = core.amount( 100 );
   create_op.policy = instant_vesting_policy_initializer{};

   const auto& curfees = *db.get_global_properties().parameters.current_fees;
   const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
   proposal_create_operation prop;
   prop.fee_paying_account = actor_id;
   prop.proposed_ops.emplace_back( create_op );
   prop.expiration_time =  db.head_block_time() + fc::days(1);
   prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

   {
      prop.expiration_time =  db.head_block_time() + fc::days(1);
      signed_transaction tx;
      tx.operations.push_back( prop );
      db.current_fee_schedule().set_fee( tx.operations.back() );
      set_expiration( db, tx );
      sign( tx, actor_private_key );
      PUSH_TX( db, tx );
   }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(white_list_asset_rewards_test)
{
   try
   {
      ACTORS((elonregistrar)(robregistrar)(elonreferrer)(robreferrer)(nathan));

      // Nathan issues white_list asset to Elon
      // Nathan issues white_list asset to Rob
      // Robreferrer added to blacklist for nathancoin asset
      // Elonregistrar added to blacklist for nathancoin2 asset
      // Elon and Rob trade in the market and pay fees
      // Check registrar/referrer rewards
      upgrade_to_lifetime_member(elonregistrar);
      upgrade_to_lifetime_member(elonreferrer);
      upgrade_to_lifetime_member(robregistrar);
      upgrade_to_lifetime_member(robreferrer);
      upgrade_to_lifetime_member(nathan);

      const account_object elon = create_account("elon", elonregistrar, elonreferrer, 20*GRAPHENE_1_PERCENT);
      const account_object rob   = create_account("rob", robregistrar, robreferrer, 20*GRAPHENE_1_PERCENT);

      fund( elon, core_asset(1000000) );
      fund( rob, core_asset(1000000) );
      fund( nathan, core_asset(2000000) );

      price price(asset(1, asset_id_type(1)), asset(1));
      constexpr auto nathancoin_market_percent = 10*GRAPHENE_1_PERCENT;
      constexpr auto nathancoin_market_percent2 = 20*GRAPHENE_1_PERCENT;
      const asset_id_type nathancoin_id = create_user_issued_asset( "NATHANCOIN", nathan, charge_market_fee|white_list, price, 0, nathancoin_market_percent ).id;
      const asset_id_type nathancoin_id2 = create_user_issued_asset( "NATHANCOIN2", nathan, charge_market_fee|white_list, price, 0, nathancoin_market_percent2 ).id;

      // Elon and Rob create some coins
      issue_uia( elon, nathancoin_id(db).amount( 200000 ) );
      issue_uia( rob, nathancoin_id2(db).amount( 200000 ) );

      constexpr auto nathancoin_reward_percent = 50*GRAPHENE_1_PERCENT;
      constexpr auto nathancoin_reward_percent2 = 50*GRAPHENE_1_PERCENT;

      update_asset(nathan_id, nathan_private_key, nathancoin_id, nathancoin_reward_percent);
      update_asset(nathan_id, nathan_private_key, nathancoin_id2, nathancoin_reward_percent2);

      BOOST_TEST_MESSAGE( "Attempting to blacklist robreferrer for nathancoin asset" );
      asset_update_blacklist_authority(nathan_id, nathancoin_id, nathan_id, nathan_private_key);
      add_account_to_blacklist(nathan_id, robreferrer_id, nathan_private_key);
      BOOST_CHECK( !(is_authorized_asset( db, robreferrer_id(db), nathancoin_id(db) )) );

      BOOST_TEST_MESSAGE( "Attempting to blacklist elonregistrar for nathancoin2 asset" );
      asset_update_blacklist_authority(nathan_id, nathancoin_id2, nathan_id, nathan_private_key);
      add_account_to_blacklist(nathan_id, elonregistrar_id, nathan_private_key);
      BOOST_CHECK( !(is_authorized_asset( db, elonregistrar_id(db), nathancoin_id2(db) )) );

      // Elon and Rob place orders which match
      create_sell_order( elon.id, nathancoin_id(db).amount(1000), nathancoin_id2(db).amount(1500) ); // Elon is willing to sell hes 1000 Nathan's for 1.5 Nathan
      create_sell_order(   rob.id, nathancoin_id2(db).amount(1500), nathancoin_id(db).amount(1000) );   // Rob is buying up to 1500 Nathan's for up to 0.6 Nathan

      // 1000 Nathans and 1500 Nathans are matched, so the fees should be
      //   100 Nathan (10%) and 300 Nathan (20%).

      // Only Rob's registrar should get rewards
      share_type rob_registrar_reward = get_market_fee_reward( rob.registrar, nathancoin_id );
      BOOST_CHECK_GT( rob_registrar_reward, 0 );
      BOOST_CHECK_EQUAL( get_market_fee_reward( rob.referrer, nathancoin_id ), 0 );
      BOOST_CHECK_EQUAL( get_market_fee_reward( elon.registrar, nathancoin_id2 ), 0 );
      BOOST_CHECK_EQUAL( get_market_fee_reward( elon.referrer, nathancoin_id2 ), 0 );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_vesting_balance_object_test )
{
   /**
    * Test checks that an account could have duplicates VBO (with the same asset_type)
    * for any type of vesting_balance_type
    * except vesting_balance_type::market_fee_sharing
   */
   try {

      ACTOR(actor);

      create_vesting_balance_object(actor_id, vesting_balance_type::unspecified);
      create_vesting_balance_object(actor_id, vesting_balance_type::unspecified);

      create_vesting_balance_object(actor_id, vesting_balance_type::cashback);
      create_vesting_balance_object(actor_id, vesting_balance_type::cashback);

      create_vesting_balance_object(actor_id, vesting_balance_type::witness);
      create_vesting_balance_object(actor_id, vesting_balance_type::witness);

      create_vesting_balance_object(actor_id, vesting_balance_type::worker);
      create_vesting_balance_object(actor_id, vesting_balance_type::worker);

      create_vesting_balance_object(actor_id, vesting_balance_type::market_fee_sharing);
      GRAPHENE_CHECK_THROW(create_vesting_balance_object(actor_id, vesting_balance_type::market_fee_sharing), fc::exception);

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_SUITE_END()
