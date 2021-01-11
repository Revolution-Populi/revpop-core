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

   void generate_blocks_past_hf1774()
   {
      generate_blocks( HARDFORK_1774_TIME );
      generate_block();
      set_expiration(db, trx);
   }

   void generate_blocks_past_hf1800()
   {
      database_fixture::generate_blocks( HARDFORK_CORE_1800_TIME );
      database_fixture::generate_block();
      set_expiration(db, trx);
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

BOOST_AUTO_TEST_CASE(cannot_create_asset_with_reward_percent_of_100_before_hf1774)
{
   try
   {
      ACTOR(issuer);

      uint16_t reward_percent = GRAPHENE_100_PERCENT + 1; // 100.01%
      flat_set<account_id_type> whitelist = {issuer_id};
      price price(asset(1, asset_id_type(1)), asset(1));
      uint16_t market_fee_percent = 100;

      additional_asset_options_t options;
      options.value.reward_percent = reward_percent;
      options.value.whitelist_market_fee_sharing = whitelist;

      GRAPHENE_CHECK_THROW(create_user_issued_asset("USD",
                                                    issuer,
                                                    charge_market_fee,
                                                    price,
                                                    2,
                                                    market_fee_percent,
                                                    options),
                           fc::assert_exception);

      reward_percent = GRAPHENE_100_PERCENT; // 100%
      options.value.reward_percent = reward_percent;
      GRAPHENE_CHECK_THROW(create_user_issued_asset("USD",
                                                    issuer,
                                                    charge_market_fee,
                                                    price,
                                                    2,
                                                    market_fee_percent,
                                                    options),
                           fc::assert_exception);

      reward_percent = GRAPHENE_100_PERCENT - 1; // 99.99%
      options.value.reward_percent = reward_percent;
      asset_object usd_asset = create_user_issued_asset("USD",
                                                        issuer,
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

BOOST_AUTO_TEST_CASE(cannot_set_reward_percent_to_100_before_hf1774)
{
   try
   {
      ACTOR(issuer);

      asset_object usd_asset = create_user_issued_asset("USD", issuer, charge_market_fee);

      uint16_t reward_percent = GRAPHENE_100_PERCENT + 1; // 100.01%
      flat_set<account_id_type> whitelist = {issuer_id};
      GRAPHENE_CHECK_THROW(
                  update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist),
                  fc::assert_exception );

      reward_percent = GRAPHENE_100_PERCENT; // 100%
      GRAPHENE_CHECK_THROW(
                  update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist),
                  fc::assert_exception );

      reward_percent = GRAPHENE_100_PERCENT - 1; // 99.99%
      update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist);

      asset_object updated_asset = usd_asset.get_id()(db);
      additional_asset_options options = updated_asset.options.extensions.value;
      BOOST_CHECK_EQUAL(reward_percent, *options.reward_percent);
      BOOST_CHECK(whitelist == *options.whitelist_market_fee_sharing);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(create_asset_with_reward_percent_of_100_after_hf1774)
{
   try
   {
      generate_blocks_past_hf1774();

      ACTOR(issuer);

      uint16_t reward_percent = GRAPHENE_100_PERCENT; // 100.00%
      flat_set<account_id_type> whitelist = {issuer_id};
      price price(asset(1, asset_id_type(1)), asset(1));
      uint16_t market_fee_percent = 100;

      additional_asset_options_t options;
      options.value.reward_percent = reward_percent;
      options.value.whitelist_market_fee_sharing = whitelist;

      asset_object usd_asset = create_user_issued_asset("USD",
                                                        issuer,
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
      ACTOR(issuer);

      asset_object usd_asset = create_user_issued_asset("USD", issuer, charge_market_fee); // make a copy

      generate_blocks_past_hf1774();

      uint16_t reward_percent = GRAPHENE_100_PERCENT; // 100.00%
      flat_set<account_id_type> whitelist = {issuer_id};
      update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist);

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
      ACTORS((jill)(izzyregistrar)(izzyreferrer)(tempregistrar));

      upgrade_to_lifetime_member(izzyregistrar);
      upgrade_to_lifetime_member(izzyreferrer);
      upgrade_to_lifetime_member(tempregistrar);

      price price(asset(1, asset_id_type(1)), asset(1));
      uint16_t market_fee_percent = 20 * GRAPHENE_1_PERCENT;
      const asset_object jillcoin = create_user_issued_asset( "JCOIN", jill, charge_market_fee,
                                                              price, 2, market_fee_percent );

      const account_object alice = create_account("alice", izzyregistrar, izzyreferrer, 50/*0.5%*/);
      const account_object bob   = create_account("bob",   izzyregistrar, izzyreferrer, 50/*0.5%*/);
      const account_object old   = create_account("old",   GRAPHENE_TEMP_ACCOUNT(db),
                                                           GRAPHENE_COMMITTEE_ACCOUNT(db), 50u);
      const account_object tmp   = create_account("tmp",   tempregistrar,
                                                           GRAPHENE_TEMP_ACCOUNT(db), 50u);

      // prepare users' balance
      issue_uia( alice, jillcoin.amount( 20000000 ) );

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
      ACTOR(issuer);
      asset_object usd_asset = create_user_issued_asset("USD", issuer, charge_market_fee);

      additional_asset_options_t options;
      options.value.reward_percent = 100;
      options.value.whitelist_market_fee_sharing = flat_set<account_id_type>{issuer_id};

      asset_update_operation update_op;
      update_op.issuer = issuer_id;
      update_op.asset_to_update = usd_asset.get_id();
      asset_options new_options;
      update_op.new_options = usd_asset.options;
      update_op.new_options.extensions = std::move(options);

      const auto& curfees = *db.get_global_properties().parameters.current_fees;
      const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
      proposal_create_operation prop;
      prop.fee_paying_account = issuer_id;
      prop.proposed_ops.emplace_back( update_op );
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

BOOST_AUTO_TEST_CASE(issue_asset){
   try
   {
       ACTORS((alice)(bob)(izzy)(jill));
      // Izzy issues asset to Alice  (Izzycoin market percent - 10%)
      // Jill issues asset to Bob    (Jillcoin market percent - 20%)

      fund( alice, core_asset(1000000) );
      fund( bob, core_asset(1000000) );
      fund( izzy, core_asset(1000000) );
      fund( jill, core_asset(1000000) );

      price price(asset(1, asset_id_type(1)), asset(1));
      constexpr auto izzycoin_market_percent = 10*GRAPHENE_1_PERCENT;
      asset_object izzycoin = create_user_issued_asset( "IZZYCOIN", izzy,  charge_market_fee, price, 2, izzycoin_market_percent );

      constexpr auto jillcoin_market_percent = 20*GRAPHENE_1_PERCENT;
      asset_object jillcoin = create_user_issued_asset( "JILLCOIN", jill,  charge_market_fee, price, 2, jillcoin_market_percent );

      // Alice and Bob create some coins
      issue_uia( alice, izzycoin.amount( 100000 ) );
      issue_uia( bob, jillcoin.amount( 100000 ) );
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

      create_vesting_balance_object(actor_id, vesting_balance_type::market_fee_sharing);
      GRAPHENE_CHECK_THROW(create_vesting_balance_object(actor_id, vesting_balance_type::market_fee_sharing), fc::exception);

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_SUITE_END()
