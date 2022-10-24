/*
 * Copyright (c) 2019 Contributors
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

/**
 * Readers of these custom active authority (CAA) tests may benefit by reviewing
 *
 * - rejection_indicator variant in restriction_predicate.hpp
 * - function_type enum in restriction.hpp
 * - GRAPHENE_OP_RESTRICTION_ARGUMENTS_VARIADIC in restriction.hpp
 */

#include <string>
#include <boost/test/unit_test.hpp>
#include <fc/exception/exception.hpp>
#include <graphene/protocol/restriction_predicate.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/custom_authority_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>

#include "../common/database_fixture.hpp"

// Dependencies required by the HTLC-related tests
#include <random>
#include <graphene/chain/htlc_object.hpp>

// Dependencies for the voting and witness tests
#include <graphene/chain/witness_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace graphene { namespace protocol {
bool operator==(const restriction& a, const restriction& b) {
   if (std::tie(a.member_index, a.restriction_type) != std::tie(b.member_index, b.restriction_type))
      return false;
   if (a.argument.is_type<void_t>())
      return b.argument.is_type<void_t>();
   using Value_Argument = static_variant<fc::typelist::slice<restriction::argument_type::list, 1>>;
   return Value_Argument::import_from(a.argument) == Value_Argument::import_from(b.argument);
}
} }


BOOST_FIXTURE_TEST_SUITE(custom_authority_tests, database_fixture)

#define FUNC(TYPE) BOOST_PP_CAT(restriction::func_, TYPE)

template<typename Object>
unsigned_int member_index(string name) {
   unsigned_int index;
   fc::typelist::runtime::for_each(typename fc::reflector<Object>::native_members(), [&name, &index](auto t) mutable {
      if (name == decltype(t)::type::get_name())
         index = decltype(t)::type::index;
   });
   return index;
}

template<typename Expression>
void expect_exception_string(const string& s, Expression e) {
   try{
      e();
      FC_THROW_EXCEPTION(fc::assert_exception, "Expected exception with string ${s}, but no exception thrown",
                         ("s", s));
   } catch (const fc::exception& e) {
      FC_ASSERT(e.to_detail_string().find(s) != string::npos, "Did not find expected string ${s} in exception: ${e}",
                ("s", s)("e", e));
   }
}
#define EXPECT_EXCEPTION_STRING(S, E) \
    BOOST_TEST_CHECKPOINT("Expect exception containing string: " S); \
    expect_exception_string(S, E)

BOOST_AUTO_TEST_CASE(restriction_predicate_tests) { try {
   using namespace graphene::protocol;
   //////
   // Create a restriction that authorizes transfers only made to Account ID 12
   //////
   vector<restriction> restrictions;
   auto to_index = member_index<transfer_operation>("to");
   restrictions.emplace_back(to_index, FUNC(eq), account_id_type(12));

   //////
   // Create an operation that transfers to Account ID 0
   // This should violate the restriction
   //////
   transfer_operation transfer;
   // Check that the proposed operation to account ID 0 is not compliant with the restriction to account ID 12
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer) == false);
   // Inspect the reasons why the proposed operation was rejected
   // The rejection path will reference portions of the restrictions
   //[
   //  {
   //    "member_index": 2,
   //    "restriction_type": 0,
   //    "argument": [
   //      7,
   //      "1.2.12"
   //    ],
   //    "extensions": []
   //  }
   //]
   BOOST_CHECK_EQUAL(restriction::restriction_count(restrictions), 1);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path.size() == 2);
   // Index 0 (the outer-most) rejection path refers to the first and only outer-most sub-restriction
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[0].get<size_t>() == 0);
   // Index 1 (the inner-most) rejection path refers to the first and only argument for an account ID of 1.2.12
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[1].get<predicate_result::rejection_reason>() == predicate_result::predicate_was_false);

   //////
   // Create an operation that transfer to Account ID 12
   // This should satisfy the restriction
   //////
   transfer.to = account_id_type(12);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer) == true);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path.size() == 0);


   //////
   // Create an INVALID restriction that references an invalid member index
   // (Index 6 is greater than the highest 0-based index of 5)
   // of the transfer operation
   //////
   restrictions.front() = restriction(fc::typelist::length<fc::reflector<transfer_operation>::native_members>(),
                                      FUNC(eq), account_id_type(12));
   //[
   //  {
   //    "member_index": 6,
   //    "restriction_type": 0,
   //    "argument": [
   //      7,
   //      "1.2.12"
   //    ],
   //    "extensions": []
   //  }
   //]
   //
   // This restriction should throw an exception related to an invalid member index
   //   10 assert_exception: Assert Exception
   //   r.member_index < typelist::length<member_list>(): Invalid member index 6 for object graphene::protocol::transfer_operation
   //           {"I":6,"O":"graphene::protocol::transfer_operation"}
   //   th_a  restriction_predicate.hxx:493 create_field_predicate
   BOOST_CHECK_THROW(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value),
                     fc::assert_exception);


   //////
   // Create an INVALID restriction that compares a transfer operation's account ID type to an asset ID type
   //////
   restrictions.front() = restriction(to_index, FUNC(eq), asset_id_type(12));
   //[
   //  {
   //    "member_index": 2,
   //    "restriction_type": 0,
   //    "argument": [
   //      8,
   //      "1.3.12"
   //    ],
   //    "extensions": []
   //  }
   //]
   //
   // This restriction should throw an exception related to invalid type
   //   10 assert_exception: Assert Exception
   //   Invalid types for predicate
   //   {}
   //   th_a  restriction_predicate.hxx:147 predicate_invalid
   //
   //   {"fc::get_typename<Field>::name()":"graphene::protocol::account_id_type","func":"func_eq","arg":[8,"1.3.12"]}
   //   th_a  restriction_predicate.hxx:476 create_predicate_function
   BOOST_CHECK_THROW(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value),
                     fc::assert_exception);

   //////
   // Create a restriction such that the operation fee must be paid with Asset ID 0
   //////
   auto fee_index = member_index<transfer_operation>("fee");
   auto asset_id_index = member_index<asset>("asset_id");
   restrictions.front() = restriction(fee_index, FUNC(attr),
                                      vector<restriction>{restriction(asset_id_index, FUNC(eq), asset_id_type(0))});

   //////
   // Check the transfer operation that pays the fee with Asset ID 0
   // This should satisfy the restriction.
   //////
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer) == true);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path.size() == 0);

   //////
   // Change the restriction such that the operation fee must be paid with Asset ID 1
   //////
   restrictions.front().argument.get<vector<restriction>>().front().argument = asset_id_type(1);
   //[
   //  {
   //    "member_index": 0,
   //    "restriction_type": 10,
   //    "argument": [
   //      39,
   //      [
   //        {
   //          "member_index": 1,
   //          "restriction_type": 0,
   //          "argument": [
   //            8,
   //            "1.3.1"
   //          ],
   //          "extensions": []
   //        }
   //      ]
   //    ],
   //    "extensions": []
   //  }
   //]

   BOOST_CHECK_EQUAL(restriction::restriction_count(restrictions), 2);
   //////
   // Check the transfer operation that pays the fee with Asset ID 0 against the restriction.
   // This should violate the restriction.
   //////
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer) == false);
   // Inspect the reasons why the proposed operation was rejected
   // The rejection path will reference portions of the restrictions
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path.size() == 3);
   // Index 0 (the outer-most) rejection path refers to the first and only outer-most sub-restriction
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[0].get<size_t>() == 0);
   // Index 1 rejection path refers to the first and only attribute of the restriction
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[1].get<size_t>() == 0);
   // Index 2 (the inner-most) rejection path refers to the expected rejection reason
   // The rejection reason should be that the predicate was false
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[2].get<predicate_result::rejection_reason>() == predicate_result::predicate_was_false);

   //////
   // Create a restriction that authorizes transfers only to Account ID 12
   //////
   restrictions.emplace_back(to_index, FUNC(eq), account_id_type(12));
   //[
   //  {
   //    "member_index": 0,
   //    "restriction_type": 10,
   //    "argument": [
   //      39,
   //      [
   //        {
   //          "member_index": 1,
   //          "restriction_type": 0,
   //          "argument": [
   //            8,
   //            "1.3.1"
   //          ],
   //          "extensions": []
   //        }
   //      ]
   //    ],
   //    "extensions": []
   //  },
   //  {
   //    "member_index": 2,
   //    "restriction_type": 0,
   //    "argument": [
   //      7,
   //      "1.2.12"
   //    ],
   //    "extensions": []
   //  }
   //]
   BOOST_CHECK_EQUAL(restriction::restriction_count(restrictions), 3);

   //////
   // Create a transfer operation that authorizes transfer to Account ID 12
   // This operation should satisfy the restriction
   //////
   transfer.to = account_id_type(12);
   transfer.fee.asset_id = asset_id_type(1);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer) == true);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path.size() == 0);

   //////
   // Create a transfer operation that transfers to Account ID 10
   // This operation should violate the restriction
   //////
   transfer.to = account_id_type(10);
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer) == false);
   // Inspect the reasons why the proposed operation was rejected
   // The rejection path will reference portions of the restrictions
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path.size() == 2);
   // Index 0 (the outer-most) rejection path refers to the first and only outer-most sub-restriction
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[0].get<size_t>() == 1);
   // Index 1 (the inner-most) rejection path refers to the first and only argument
   BOOST_CHECK(get_restriction_predicate(restrictions, operation::tag<transfer_operation>::value)(transfer)
               .rejection_path[1].get<predicate_result::rejection_reason>() == predicate_result::predicate_was_false);

   //////
   // Create a restriction where the ext.owner_special_authority field is unspecified
   //////
   restrictions.clear();
   auto extensions_index = member_index<account_update_operation>("extensions");
   auto authority_index = member_index<account_update_operation::ext>("owner_special_authority");
   restrictions.emplace_back(extensions_index, FUNC(attr),
                             vector<restriction>{restriction(authority_index, FUNC(eq), void_t())});
   //[
   //  {
   //    "member_index": 5,
   //    "restriction_type": 10,
   //    "argument": [
   //      39,
   //      [
   //        {
   //          "member_index": 1,
   //          "restriction_type": 0,
   //          "argument": [
   //            0,
   //            {}
   //          ],
   //          "extensions": []
   //        }
   //      ]
   //    ],
   //    "extensions": []
   //  }
   //]
   BOOST_CHECK_EQUAL(restriction::restriction_count(restrictions), 2);
   auto predicate = get_restriction_predicate(restrictions, operation::tag<account_update_operation>::value);

   //////
   // Create an account update operation without any owner_special_authority extension
   //////
   account_update_operation update;
   // The transfer operation should violate the restriction
   BOOST_CHECK_THROW(predicate(transfer), fc::assert_exception);
   // The update operation should satisfy the restriction
   BOOST_CHECK(predicate(update) == true);
   BOOST_CHECK(predicate(update).rejection_path.size() == 0);

   //////
   // Change the update operation to include an owner_special_authority
   // This should violate the restriction
   //////
   update.extensions.value.owner_special_authority = special_authority();
   BOOST_CHECK(predicate(update) == false);
   BOOST_CHECK_EQUAL(predicate(update).rejection_path.size(), 3);
   // Index 0 (the outer-most) rejection path refers to the first and only restriction
   BOOST_CHECK(predicate(update).rejection_path[0].get<size_t>() == 0);
   // Index 1 rejection path refers to the first and only attribute of the restriction
   BOOST_CHECK(predicate(update).rejection_path[1].get<size_t>() == 0);
   // Index 2 (the inner-most) rejection path refers to the expected rejection reason
   // The rejection reason should be that the predicate was false
   BOOST_CHECK(predicate(update).rejection_path[2].get<predicate_result::rejection_reason>() ==
               predicate_result::predicate_was_false);

   //////
   // Change the restriction where the ext.owner_special_authority field must be specified
   //////
   restrictions.front().argument.get<vector<restriction>>().front().restriction_type = FUNC(ne);
   //[
   //  {
   //    "member_index": 5,
   //    "restriction_type": 10,
   //    "argument": [
   //      39,
   //      [
   //        {
   //          "member_index": 1,
   //          "restriction_type": 1,
   //          "argument": [
   //            0,
   //            {}
   //          ],
   //          "extensions": []
   //        }
   //      ]
   //    ],
   //    "extensions": []
   //  }
   //]

   //////
   // The update operation should satisfy the new restriction because the ext.owner_special_authority is specified
   //////
   predicate = get_restriction_predicate(restrictions, operation::tag<account_update_operation>::value);
   BOOST_CHECK(predicate(update) == true);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(container_in_not_in_checks) { try {
   vector<restriction> restrictions;
   restrictions.emplace_back(member_index<asset_update_feed_producers_operation>("new_feed_producers"), FUNC(in),
                             flat_set<account_id_type>{account_id_type(5), account_id_type(6), account_id_type(7)});
   auto pred = get_restriction_predicate(restrictions, operation::tag<asset_update_feed_producers_operation>::value);

   asset_update_feed_producers_operation op;
   BOOST_CHECK(pred(op));
   op.new_feed_producers = {account_id_type(1)};
   BOOST_CHECK(!pred(op));
   op.new_feed_producers = {account_id_type(5)};
   BOOST_CHECK(pred(op));
   op.new_feed_producers = {account_id_type(5), account_id_type(6)};
   BOOST_CHECK(pred(op));
   op.new_feed_producers = {account_id_type(5), account_id_type(6), account_id_type(7)};
   BOOST_CHECK(pred(op));
   op.new_feed_producers = {account_id_type(1), account_id_type(5), account_id_type(6), account_id_type(7)};
   BOOST_CHECK(!pred(op));
   op.new_feed_producers = {account_id_type(5), account_id_type(6), account_id_type(7), account_id_type(8)};
   BOOST_CHECK(!pred(op));

   restrictions.front().restriction_type = FUNC(not_in);
   pred = get_restriction_predicate(restrictions, operation::tag<asset_update_feed_producers_operation>::value);
   op.new_feed_producers.clear();
   BOOST_CHECK(pred(op));
   op.new_feed_producers = {account_id_type(1)};
   BOOST_CHECK(pred(op));
   op.new_feed_producers = {account_id_type(5)};
   BOOST_CHECK(!pred(op));
   op.new_feed_producers = {account_id_type(5), account_id_type(6)};
   BOOST_CHECK(!pred(op));
   op.new_feed_producers = {account_id_type(5), account_id_type(6), account_id_type(7)};
   BOOST_CHECK(!pred(op));
   op.new_feed_producers = {account_id_type(1), account_id_type(5), account_id_type(6), account_id_type(7)};
   BOOST_CHECK(!pred(op));
   op.new_feed_producers = {account_id_type(5), account_id_type(6), account_id_type(7), account_id_type(8)};
   BOOST_CHECK(!pred(op));
} FC_LOG_AND_RETHROW() }

   /**
    * Test predicates containing logical ORs
    * Test of authorization and revocation of one account (Nathan) authorizing multiple other accounts (Bob and Charlie)
    * to transfer out of her account by using a single custom active authority with two logical OR branches.
    *
    * This can alternatively be achieved by using two custom active authority authorizations
    * as is done in multiple_transfer_custom_auths
    */
   BOOST_AUTO_TEST_CASE(logical_or_transfer_predicate_tests) {
      try {
         using namespace graphene::protocol;
         //////
         // Create a restriction that authorizes transfers only made to Account ID 12 or Account 15
         //////
         auto to_index = member_index<transfer_operation>("to");
         vector<restriction> branch1 = vector<restriction>{restriction(to_index, FUNC(eq), account_id_type(12))};
         vector<restriction> branch2 = vector<restriction>{restriction(to_index, FUNC(eq), account_id_type(15))};
         unsigned_int dummy_index = 999;
         vector<restriction> or_restrictions = {
                 restriction(dummy_index, FUNC(logical_or), vector<vector<restriction>>{branch1, branch2})};
         //[
         //  {
         //    "member_index": 999,
         //    "restriction_type": 11,
         //    "argument": [
         //      40,
         //      [
         //        [
         //          {
         //            "member_index": 2,
         //            "restriction_type": 0,
         //            "argument": [
         //              7,
         //              "1.2.12"
         //            ],
         //            "extensions": []
         //          }
         //        ],
         //        [
         //          {
         //            "member_index": 2,
         //            "restriction_type": 0,
         //            "argument": [
         //              7,
         //              "1.2.15"
         //            ],
         //            "extensions": []
         //          }
         //        ]
         //      ]
         //    ],
         //    "extensions": []
         //  }
         //]
         BOOST_CHECK_EQUAL(restriction::restriction_count(or_restrictions), 3);
         auto predicate = get_restriction_predicate(or_restrictions, operation::tag<transfer_operation>::value);

         //////
         // Create an operation that transfers to Account ID 12
         // This should satisfy the restriction because Account ID 12 is authorized to transfer
         //////
         transfer_operation transfer_to_12 = transfer_operation();
         transfer_to_12.to = account_id_type(12);
         BOOST_CHECK_EQUAL(predicate(transfer_to_12).success, true);
         BOOST_CHECK_EQUAL(predicate(transfer_to_12).rejection_path.size(), 0);

         //////
         // Create an operation that transfers to Account ID 15
         // This should satisfy the restriction because Account ID 15 is authorized to transfer
         //////
         transfer_operation transfer_to_15 = transfer_operation();
         transfer_to_15.to = account_id_type(15);
         BOOST_CHECK(predicate(transfer_to_15) == true);
         BOOST_CHECK_EQUAL(predicate(transfer_to_15).rejection_path.size(), 0);

         //////
         // Create an operation that transfers to Account ID 1
         // This should violate the restriction because Account 1 is not authorized to transfer
         //////
         transfer_operation transfer_to_1;
         transfer_to_1.to = account_id_type(1);
         BOOST_CHECK(predicate(transfer_to_1) == false);

         // JSON-formatted Rejection path
         //[ // A vector of predicate results
         //  [
         //    0, // Index 0 (the outer-most) rejection path
         //    0  // The first and only outer-most sub-restriction
         //  ],
         //  [
         //    1,  // Index 1 (the inner-most) rejection path
         //    [  // A vector of predicate results
         //      {
         //        "success": false,
         //        "rejection_path": [
         //          [
         //            0, // Index 0 (the outer-most) rejection path
         //            0  // Restriction 1 along this branch
         //          ],
         //          [
         //            2, // Rejection reason
         //            "predicate_was_false"
         //          ]
         //        ]
         //      },
         //      {
         //        "success": false,
         //        "rejection_path": [
         //          [
         //            0, // Index 0 (the outer-most) rejection path
         //            0  // Restriction 1 along this branch
         //          ],
         //          [
         //            2, // Rejection reason
         //            "predicate_was_false"
         //          ]
         //        ]
         //      }
         //    ]
         //  ]
         //]

         // C++ style check of the rejection path
         BOOST_CHECK_EQUAL(predicate(transfer_to_1).rejection_path.size(), 2);
         // Index 0 (the outer-most) rejection path refers to  and only outer-most sub-restriction
         BOOST_CHECK(predicate(transfer_to_1).rejection_path[0].get<size_t>() == 0);
         // Index 1 (the inner-most) rejection path refers to the first and only argument:
         // the vector of branches each of which are one level deep
         vector<predicate_result> branch_results = predicate(
                 transfer_to_1).rejection_path[1].get<vector<predicate_result>>();
         unsigned long nbr_branches = branch_results.size();
         BOOST_CHECK_EQUAL(nbr_branches, 2);
         for (unsigned long j = 0; j < nbr_branches; ++j) {
            predicate_result &result = branch_results.at(j);
            BOOST_CHECK_EQUAL(result.success, false);

            BOOST_CHECK_EQUAL(result.rejection_path.size(), 2);
            // Index 0 (the outer-most) rejection path refers to the first and only restriction
            BOOST_CHECK_EQUAL(result.rejection_path[0].get<size_t>(), 0);
            // Index 1 (the inner-most) rejection path refers to the first and only argument for an account ID:
            // either 1.2.12 or 1.2.15
            BOOST_CHECK(result.rejection_path[1].get<predicate_result::rejection_reason>() ==
                        predicate_result::predicate_was_false);

         }

      } FC_LOG_AND_RETHROW()
   }


BOOST_AUTO_TEST_CASE(custom_auths) { try {
   //////
   // Initialize the test
   //////
   generate_blocks(HARDFORK_BSIP_40_TIME);
   generate_blocks(5);
   db.modify(global_property_id_type()(db), [](global_property_object& gpo) {
      gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
   });
   set_expiration(db, trx);
   ACTORS((nathan)(bob))
   fund(nathan, asset(1000*GRAPHENE_BLOCKCHAIN_PRECISION));
   fund(bob, asset(1000*GRAPHENE_BLOCKCHAIN_PRECISION));

   //////
   // Create a custom authority where Bob is authorized to transfer from Nathan's account
   // if and only if the transfer amount is less than 100 of Asset ID 0.
   // This custom authority is NOT YET published.
   //////
   custom_authority_create_operation op;
   op.account = nathan.get_id();
   op.auth.add_authority(bob.get_id(), 1);
   op.auth.weight_threshold = 1;
   op.enabled = true;
   op.valid_to = db.head_block_time() + 1000;
   op.operation_type = operation::tag<transfer_operation>::value;
   auto transfer_amount_index = member_index<transfer_operation>("amount");
   auto asset_amount_index = member_index<asset>("amount");
   auto asset_id_index = member_index<asset>("asset_id");
   op.restrictions = {restriction(transfer_amount_index, restriction::func_attr, vector<restriction>{
                          restriction(asset_amount_index, restriction::func_lt,
                                      int64_t(100*GRAPHENE_BLOCKCHAIN_PRECISION)),
                          restriction(asset_id_index, restriction::func_eq, asset_id_type(0))})};
   //[
   //  {
   //    "member_index": 3,
   //    "restriction_type": 10,
   //    "argument": [
   //      39,
   //      [
   //        {
   //          "member_index": 0,
   //          "restriction_type": 2,
   //          "argument": [
   //            2,
   //            10000000
   //          ],
   //          "extensions": []
   //        },
   //        {
   //          "member_index": 1,
   //          "restriction_type": 0,
   //          "argument": [
   //            8,
   //            "1.3.0"
   //          ],
   //          "extensions": []
   //        }
   //      ]
   //    ],
   //    "extensions": []
   //  }
   //]
   BOOST_CHECK_EQUAL(restriction::restriction_count(op.restrictions), 3);


   //////
   // Bob attempts to transfer 99 CORE from Nathan's account
   // This attempt should fail because it is attempted before the custom authority is published
   //////
   transfer_operation top;
   top.to = bob.get_id();
   top.from = nathan.get_id();
   top.amount.amount = 99 * GRAPHENE_BLOCKCHAIN_PRECISION;
   trx.operations = {top};
   sign(trx, bob_private_key);
   // No custom auth yet; bob's transfer should reject
   BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);

   //////
   // Nathan publishes the custom authority
   //////
   trx.clear();
   trx.operations = {op};
   sign(trx, nathan_private_key);
   PUSH_TX(db, trx);

   custom_authority_id_type auth_id =
           db.get_index_type<custom_authority_index>().indices().get<by_account_custom>().find(nathan_id)->id;

   //////
   // Bob attempts to transfer 99 CORE from Nathan's account
   // This attempt should succeed because it is attempted after the custom authority is published
   //////
   trx.clear();
   trx.operations = {top};
   sign(trx, bob_private_key);
   PUSH_TX(db, trx);

   //////
   // Bob attempts to transfer 100 CORE from Nathan's account
   // This attempt should fail because it exceeds the authorized amount
   //////
   trx.operations.front().get<transfer_operation>().amount.amount = 100*GRAPHENE_BLOCKCHAIN_PRECISION;
   trx.clear_signatures();
   sign(trx, bob_private_key);
   // If bob tries to transfer 100, it rejects because the restriction is strictly less than 100
   EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

   //////
   // Update the custom authority so that Bob is authorized to transfer from Nathan's account
   // if and only if the transfer amount EXACTLY EQUALS 100 of Asset ID 0.
   // This custom authority is NOT YET published.
   //////
   op.restrictions.front().argument.get<vector<restriction>>().front().restriction_type = restriction::func_eq;
   custom_authority_update_operation uop;
   uop.account = nathan.get_id();
   uop.authority_to_update = auth_id;
   uop.restrictions_to_remove = {0};
   uop.restrictions_to_add = {op.restrictions.front()};
   trx.clear();
   trx.operations = {uop};
   sign(trx, nathan_private_key);
   PUSH_TX(db, trx);

   BOOST_CHECK(auth_id(db).get_restrictions() == uop.restrictions_to_add);

   //////
   // Bob attempts to transfer 99 CORE from Nathan's account
   // This attempt should fail because only transfers of 100 CORE are authorized
   //////
   trx.clear();
   trx.operations = {top};
   trx.expiration += 5;
   sign(trx, bob_private_key);
   // The transfer of 99 should reject because the requirement is for exactly 100
   EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

   //////
   // Bob attempts to transfer 100 CORE from Nathan's account
   // This attempt should succeed because transfers of exactly 100 CORE are authorized by Nathan
   //////
   trx.operations.front().get<transfer_operation>().amount.amount = 100*GRAPHENE_BLOCKCHAIN_PRECISION;
   trx.clear_signatures();
   sign(trx, bob_private_key);
   PUSH_TX(db, trx);
   auto transfer = trx;

   generate_block();

   //////
   // Bob attempts to transfer 100 CORE from Nathan's account AGAIN
   // This attempt should succeed because there are no limits to the quantity of transfers
   // besides potentially depleting the CORE in Nathan's account
   //////
   trx.expiration += 5;
   trx.clear_signatures();
   sign(trx, bob_private_key);
   PUSH_TX(db, trx);

   //////
   // Nathan revokes the custom authority for Bob
   //////
   custom_authority_delete_operation dop;
   dop.account = nathan.get_id();
   dop.authority_to_delete = auth_id;
   trx.clear();
   trx.operations = {dop};
   sign(trx, nathan_private_key);
   PUSH_TX(db, trx);

   //////
   // Bob attempts to transfer 100 CORE from Nathan's account
   // This attempt should fail because it is attempted after the custom authority has been revoked
   //////
   transfer.expiration += 10;
   transfer.clear_signatures();
   sign(transfer, bob_private_key);
   BOOST_CHECK_THROW(PUSH_TX(db, transfer), tx_missing_active_auth);
} FC_LOG_AND_RETHROW() }


   /**
    * Test of authorization and revocation of one account (Nathan) authorizing multiple other accounts (Bob and Charlie)
    * to transfer out of her account by using two distinct custom active authorities.
    *
    * This can alternatively be achieved by using a single custom active authority with two logical OR branches
    * as is done in logical_or_transfer_predicate_tests
    */
   BOOST_AUTO_TEST_CASE(multiple_transfer_custom_auths) {
      try {
         //////
         // Initialize the test
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);
         ACTORS((nathan)(bob)(charlie)(diana))
         fund(nathan, asset(1000 * GRAPHENE_BLOCKCHAIN_PRECISION));
         fund(bob, asset(1000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Charlie
         // This attempt should fail because Nathan has not authorized anyone to transfer from her account
         //////
         transfer_operation bob_transfers_from_nathan_to_charlie;
         bob_transfers_from_nathan_to_charlie.to = charlie.get_id();
         bob_transfers_from_nathan_to_charlie.from = nathan.get_id();
         bob_transfers_from_nathan_to_charlie.amount.amount = 100 * GRAPHENE_BLOCKCHAIN_PRECISION;
         trx.operations = {bob_transfers_from_nathan_to_charlie};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);

         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should fail because Nathan has not authorized anyone to transfer from her account
         //////
         transfer_operation bob_transfers_from_nathan_to_diana;
         bob_transfers_from_nathan_to_diana.to = diana.get_id();
         bob_transfers_from_nathan_to_diana.from = nathan.get_id();
         bob_transfers_from_nathan_to_diana.amount.amount = 60 * GRAPHENE_BLOCKCHAIN_PRECISION;
         trx.operations = {bob_transfers_from_nathan_to_diana};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);

         //////
         // Charlie attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should fail because Nathan has not authorized anyone to transfer from her account
         //////
         transfer_operation charlie_transfers_from_nathan_to_diana;
         charlie_transfers_from_nathan_to_diana.to = diana.get_id();
         charlie_transfers_from_nathan_to_diana.from = nathan.get_id();
         charlie_transfers_from_nathan_to_diana.amount.amount = 25 * GRAPHENE_BLOCKCHAIN_PRECISION;
         trx.operations = {charlie_transfers_from_nathan_to_diana};
         sign(trx, charlie_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);

         //////
         // Create a custom authority where Bob is authorized to transfer from Nathan's account to Charlie
         //////
         custom_authority_create_operation op;
         op.account = nathan.get_id();
         op.auth.add_authority(bob.get_id(), 1);
         op.auth.weight_threshold = 1;
         op.enabled = true;
         op.valid_to = db.head_block_time() + 1000;
         op.operation_type = operation::tag<transfer_operation>::value;
         auto to_index = member_index<transfer_operation>("to");
         vector<restriction> restrictions;
         restrictions.emplace_back(to_index, FUNC(eq), charlie.get_id());
         op.restrictions = restrictions;
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 0,
         //    "argument": [
         //      7,
         //      "1.2.18"
         //    ],
         //    "extensions": []
         //  }
         //]

         // Nathan publishes the custom authority
         trx.clear();
         trx.operations = {op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);

         custom_authority_id_type ca_bob_transfers_from_nathan_to_charlie =
                 db.get_index_type<custom_authority_index>().indices().get<by_account_custom>().find(nathan_id)->id;

         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Charlie
         // This attempt should succeed because it is attempted after the custom authority is published
         //////
         trx.clear();
         trx.operations = {bob_transfers_from_nathan_to_charlie};
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);

         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should fail because Nathan has not authorized Bob to transfer to Diana
         //////
         trx.clear();
         trx.operations = {bob_transfers_from_nathan_to_diana};
         sign(trx, charlie_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // {"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

         //////
         // Charlie attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should fail because Nathan has not authorized Charlie to transfer to Diana
         //////
         trx.clear();
         trx.operations = {charlie_transfers_from_nathan_to_diana};
         sign(trx, charlie_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // {"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Advance the blockchain to generate distinctive hash IDs for the re-used transactions
         //////
         generate_blocks(1);

         //////
         // Create a custom authority where Charlie is authorized to transfer from Nathan's account to Diana
         //////
         op = custom_authority_create_operation();
         op.account = nathan.get_id();
         op.auth.add_authority(charlie.get_id(), 1);
         op.auth.weight_threshold = 1;
         op.enabled = true;
         op.valid_to = db.head_block_time() + 1000;
         op.operation_type = operation::tag<transfer_operation>::value;
         restrictions.clear();
         restrictions.emplace_back(to_index, FUNC(eq), diana.get_id());
         op.restrictions = restrictions;
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 0,
         //    "argument": [
         //      7,
         //      "1.2.19"
         //    ],
         //    "extensions": []
         //  }
         //]

         // Nathan publishes the additional custom authority
         trx.clear();
         trx.operations = {op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);

         // Note the additional custom authority
         const auto &ca_index = db.get_index_type<custom_authority_index>().indices().get<by_account_custom>();

         auto ca_nathan_range = ca_index.equal_range(nathan_id);
         long nbr_nathan_auths = std::distance(ca_nathan_range.first, ca_nathan_range.second);
         BOOST_CHECK_EQUAL(2, nbr_nathan_auths);
         auto iter = ca_nathan_range.first;
         custom_authority_id_type *ca_charlie_transfers_from_nathan_to_diana = nullptr;
         while (iter != ca_index.end()) {
            custom_authority_id_type ca_id = iter->id;
            const custom_authority_object *ca = db.find<custom_authority_object>(ca_id);
            flat_map<account_id_type, weight_type> ca_authorities = ca->auth.account_auths;
            BOOST_CHECK_EQUAL(1, ca_authorities.size());
            if (ca_authorities.find(charlie.get_id()) != ca_authorities.end()) {
               ca_charlie_transfers_from_nathan_to_diana = &ca_id;
               break;
            }

            iter++;
         }
         BOOST_CHECK(ca_charlie_transfers_from_nathan_to_diana != nullptr);

         //////
         // Charlie attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should succeed because it is attempted after the custom authority is published
         //////
         trx.clear();
         trx.operations = {charlie_transfers_from_nathan_to_diana};
         sign(trx, charlie_private_key);
         PUSH_TX(db, trx);

         //////
         // Bob should still be able to transfer from Nathan to Charlie
         // Bob attempts to transfer 100 CORE from Nathan's account to Charlie
         // This attempt should succeed because it was previously authorized by Nathan
         //////
         trx.clear();
         trx.operations = {bob_transfers_from_nathan_to_charlie};
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);

         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should fail because Nathan has not authorized Bob to transfer to Diana
         //////
         trx.clear();
         trx.operations = {bob_transfers_from_nathan_to_diana};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path for the first custom authority
         // "rejected_custom_auths":[["1.17.0",[0,{"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}]]]
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         // Check for reference to the second CAA 1.17.0
         // "rejected_custom_auths":[["1.17.0",[0,{"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}]]]
         EXPECT_EXCEPTION_STRING("1.17.0", [&] {PUSH_TX(db, trx);});


         //////
         // Advance the blockchain to generate distinctive hash IDs for the re-used transactions
         //////
         generate_blocks(1);

         //////
         // Nathan revokes the custom authority for Bob
         //////
         custom_authority_delete_operation revoke_bob_authorization;
         revoke_bob_authorization.account = nathan.get_id();
         revoke_bob_authorization.authority_to_delete = ca_bob_transfers_from_nathan_to_charlie;
         trx.clear();
         trx.operations = {revoke_bob_authorization};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);

         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Charlie
         // This attempt should fail because Nathan has revoked authorization for Bob to transfer from her account
         //////
         trx.clear();
         trx.operations = {bob_transfers_from_nathan_to_charlie};
         sign(trx, bob_private_key);
         // General check of the exception
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // Check the rejection path
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         // Check for reference to the second CAA 1.17.1
         // "rejected_custom_auths":[["1.17.1",[0,{"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}]]]
         EXPECT_EXCEPTION_STRING("1.17.1", [&] {PUSH_TX(db, trx);});

         //////
         // Charlie attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should succeed because Nathan should still be authorized to transfer from Nathan account
         //////
         trx.clear();
         trx.operations = {charlie_transfers_from_nathan_to_diana};
         sign(trx, charlie_private_key);
         PUSH_TX(db, trx);

         //////
         // Bob attempts to transfer 100 CORE from Nathan's account to Diana
         // This attempt should fail because Nathan has not authorized Bob to transfer to Diana
         //////
         trx.clear();
         trx.operations = {bob_transfers_from_nathan_to_diana};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of authorization of one account (faucet) authorizing another key
    * to register accounts
    */
   BOOST_AUTO_TEST_CASE(authorized_faucet_other_key_custom_auths) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: faucet account
         //////
         ACTORS((faucet)(charlie));
         fund(faucet, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));
         account_upgrade_operation uop;
         uop.account_to_upgrade = faucet.get_id();
         uop.upgrade_to_lifetime_member = true;
         trx.clear();
         trx.operations.emplace_back(std::move(uop));
         sign(trx, faucet_private_key);
         PUSH_TX(db, trx);

         // Lambda for creating account
         auto create_account_by_name = [&](const string &name, const account_object& registrar) {
            account_create_operation create_op;
            create_op.name = name;
            public_key_type new_key = public_key_type(generate_private_key(name + " seed").get_public_key());
            create_op.registrar = registrar.id;
            create_op.owner = authority(1, new_key, 1);
            create_op.active = authority(1, new_key, 1);
            create_op.options.memo_key = new_key;
            create_op.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;

            return create_op;
         };


         //////
         // Attempt to register an account with this key
         // This should succeed because faucet is a lifetime member account
         //////
         string name = "account1";
         account_create_operation create_op = create_account_by_name(name, faucet);
         trx.clear();
         trx.operations = {create_op};
         sign(trx, faucet_private_key);
         PUSH_TX(db, trx);


         //////
         // Define a key that can be authorized
         // This can be a new key or an existing key. The existing key may even be the active key of an account.
         //////
         fc::ecc::private_key some_private_key = generate_private_key("some key");
         public_key_type some_public_key = public_key_type(some_private_key.get_public_key());


         //////
         // Attempt to register an account with this key
         // This should fail because the key is not authorized to register any accounts
         //////
         name = "account2";
         create_op = create_account_by_name(name, faucet);
         trx.clear();
         trx.operations = {create_op};
         sign(trx, some_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);


         //////
         // faucet authorizes a key to register accounts on its behalf
         //////
         custom_authority_create_operation authorize_account_registration;
         authorize_account_registration.account = faucet.get_id();
         authorize_account_registration.auth.add_authority(some_public_key, 1);
         authorize_account_registration.auth.weight_threshold = 1;
         authorize_account_registration.enabled = true;
         authorize_account_registration.valid_to = db.head_block_time() + 1000;
         authorize_account_registration.operation_type = operation::tag<account_create_operation>::value;
         trx.clear();
         trx.operations = {authorize_account_registration};
         sign(trx, faucet_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate a distinctive hash ID for the account registration transaction
         //////
         generate_blocks(1);


         //////
         // Attempt to register an account with this key
         // This should succeed because the key is authorized to register any accounts
         //////
         trx.clear();
         trx.operations.emplace_back(std::move(create_op));
         sign(trx, some_private_key);
         PUSH_TX(db, trx);


         //////
         // Attempt to register an account with this key
         // This should succeed because the key is authorized to register any accounts
         //////
         create_op = create_account_by_name("account3", faucet);
         trx.clear();
         trx.operations = {create_op};
         sign(trx, some_private_key);
         PUSH_TX(db, trx);


         //////
         // Attempt to transfer funds out of the faucet account
         // This should fail because the key is not authorized to transfer from the faucet account
         //////
         transfer_operation top;
         top.amount.amount = 99 * GRAPHENE_BLOCKCHAIN_PRECISION;
         top.from = faucet.get_id();
         top.to = charlie.get_id();
         top.fee.asset_id = asset_id_type(1);
         trx.clear();
         trx.operations = {top};
         sign(trx, some_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});


         //////
         // Attempt to register an account with this key
         // This should succeed because the key is authorized to register any accounts
         //////
         create_op = create_account_by_name("account4", faucet);
         trx.clear();
         trx.operations = {create_op};
         sign(trx, some_private_key);
         PUSH_TX(db, trx);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of not equal (ne) restriction on an operation field
    * Test of CAA for asset_issue_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to issue an asset (NATHANCOIN) to any account except a banned account (banned1)
    */
   BOOST_AUTO_TEST_CASE(authorized_asset_issue_exceptions_1) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(allowed1)(allowed2)(banned1)(allowed3));
         fund(nathan, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         // Lambda for issuing an asset to an account
         auto issue_amount_to = [&](const account_id_type &issuer, const asset &amount, const account_id_type &to) {
            asset_issue_operation op;
            op.issuer = issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = to;

            return op;
         };

         //////
         // Create a UIA
         //////
         upgrade_to_lifetime_member(nathan);
         create_user_issued_asset("NATHANCOIN", nathan, white_list);
         create_user_issued_asset("SPECIALCOIN", nathan,  white_list);
         generate_blocks(1);
         const asset_object &nathancoin = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("NATHANCOIN");
         const asset_object &specialcoin = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("SPECIALCOIN");
         const asset_id_type nathancoin_id = nathancoin.id;


         //////
         // Attempt to issue the UIA to an account with the Nathan key
         // This should succeed because Nathan is the issuer
         //////
         asset_issue_operation issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin_id), allowed1.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Bob attempts to issue the UIA to an allowed account
         // This should fail because Bob is not authorized to issue any NATHANCOIN
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin_id), allowed2.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});


         //////
         // Nathan authorizes Bob to issue assets on its behalf
         // except for account banned1
         //////
         custom_authority_create_operation authorize_to_issue;
         authorize_to_issue.account = nathan.get_id();
         authorize_to_issue.auth.add_authority(bob.get_id(), 1);
         authorize_to_issue.auth.weight_threshold = 1;
         authorize_to_issue.enabled = true;
         authorize_to_issue.valid_to = db.head_block_time() + 1000;
         authorize_to_issue.operation_type = operation::tag<asset_issue_operation>::value;

         auto asset_index = member_index<asset_issue_operation>("asset_to_issue");
         auto asset_id_index = member_index<asset>("asset_id");
         authorize_to_issue.restrictions.emplace_back(restriction(asset_index, restriction::func_attr, vector<restriction>{
                 restriction(asset_id_index, restriction::func_eq, nathancoin_id)}));
         auto issue_to_index = member_index<asset_issue_operation>("issue_to_account");
         authorize_to_issue.restrictions.emplace_back(issue_to_index, FUNC(ne), banned1.get_id());
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 10,
         //    "argument": [
         //      39,
         //      [
         //        {
         //          "member_index": 1,
         //          "restriction_type": 0,
         //          "argument": [
         //            8,
         //            "1.3.2"
         //          ],
         //          "extensions": []
         //        }
         //      ]
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 3,
         //    "restriction_type": 1,
         //    "argument": [
         //      7,
         //      "1.2.20"
         //    ],
         //    "extensions": []
         //  }
         //]

         trx.clear();
         trx.operations = {authorize_to_issue};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate a distinctive hash ID for the reused operation
         //////
         generate_blocks(1);


         //////
         // Bob attempts to issue the UIA to an allowed account
         // This should succeed because Bob is now authorized to issue NATHANCOIN
         //////
         trx.clear();
         trx.operations.emplace_back(std::move(issue_op));
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);


         //////
         // Bob attempts to issue the special coin to an allowed account
         // This should fail because Bob is not authorized to issue SPECIALCOIN to any account
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, specialcoin.id), allowed3.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,0],[0,0],[2,"predicate_was_false"]
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to issue the UIA to a banned account with the Bob's key
         // This should fail because Bob is not authorized to issue NATHANCOIN to the banned account
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin_id), banned1.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,1],[2,"predicate_was_false"]
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


      } FC_LOG_AND_RETHROW()
   }




   /**
    * Test of not in (not_in) restriction on an operation field
    * Test of CAA for asset_issue_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to issue an asset (NATHANCOIN) except to 3 banned accounts (banned1, banned2, banned3)
    */
   BOOST_AUTO_TEST_CASE(authorized_asset_issue_exceptions_2) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(allowed1)(allowed2)(banned1)(banned2)(banned3)(allowed3));
         fund(nathan, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         // Lambda for issuing an asset to an account
         auto issue_amount_to = [&](const account_id_type &issuer, const asset &amount, const account_id_type &to) {
            asset_issue_operation op;
            op.issuer = issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = to;

            return op;
         };

         //////
         // Create user-issued assets
         //////
         upgrade_to_lifetime_member(nathan);
         create_user_issued_asset("NATHANCOIN", nathan, white_list);
         create_user_issued_asset("SPECIALCOIN", nathan,  white_list);
         generate_blocks(1);
         const asset_object &nathancoin = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("NATHANCOIN");
         const asset_object &specialcoin = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("SPECIALCOIN");
         const asset_id_type nathancoin_id = nathancoin.id;


         //////
         // Attempt to issue the UIA to an account with the Nathan key
         // This should succeed because Nathan is the issuer
         //////
         asset_issue_operation issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin_id), allowed1.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Bob attempts to issue the UIA to an allowed account
         // This should fail because Bob is not authorized to issue any NATHANCOIN
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin_id), allowed2.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});


         //////
         // Nathan authorizes Bob to issue assets on its behalf
         // except for accounts banned1, banned2, and banned3
         //////
         custom_authority_create_operation authorize_to_issue;
         authorize_to_issue.account = nathan.get_id();
         authorize_to_issue.auth.add_authority(bob.get_id(), 1);
         authorize_to_issue.auth.weight_threshold = 1;
         authorize_to_issue.enabled = true;
         authorize_to_issue.valid_to = db.head_block_time() + 1000;
         authorize_to_issue.operation_type = operation::tag<asset_issue_operation>::value;

         auto asset_index = member_index<asset_issue_operation>("asset_to_issue");
         auto asset_id_index = member_index<asset>("asset_id");
         authorize_to_issue.restrictions.emplace_back(restriction(asset_index, restriction::func_attr, vector<restriction>{
                 restriction(asset_id_index, restriction::func_eq, nathancoin_id)}));
         auto issue_to_index = member_index<asset_issue_operation>("issue_to_account");
         authorize_to_issue.restrictions
                 .emplace_back(issue_to_index, FUNC(not_in),
                               flat_set<account_id_type>{banned1.get_id(), banned2.get_id(), banned3.get_id()});
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 10,
         //    "argument": [
         //      39,
         //      [
         //        {
         //          "member_index": 1,
         //          "restriction_type": 0,
         //          "argument": [
         //            8,
         //            "1.3.2"
         //          ],
         //          "extensions": []
         //        }
         //      ]
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 3,
         //    "restriction_type": 7,
         //    "argument": [
         //      26,
         //      [
         //        "1.2.20",
         //        "1.2.21",
         //        "1.2.22"
         //      ]
         //    ],
         //    "extensions": []
         //  }
         //]

         trx.clear();
         trx.operations = {authorize_to_issue};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate a distinctive hash ID for the reused operation
         //////
         generate_blocks(1);


         //////
         // Bob attempts to issue the UIA to an allowed account
         // This should succeed because Bob is now authorized to issue NATHANCOIN
         //////
         trx.clear();
         trx.operations.emplace_back(std::move(issue_op));
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);


         //////
         // Bob attempts to issue the special coin to an allowed account
         // This should fail because Bob is not authorized to issue SPECIALCOIN to any account
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, specialcoin.id), allowed3.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,0],[0,0],[2,"predicate_was_false"]
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to issue the UIA to a banned account with the Bob's key
         // This should fail because Bob is not authorized to issue NATHANCOIN to banned account (banned1)
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin.id), banned1.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,1],[2,"predicate_was_false"]
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to issue the UIA to a banned account with the Bob's key
         // This should fail because Bob is not authorized to issue NATHANCOIN to banned account (banned2)
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin.id), banned2.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,1],[2,"predicate_was_false"]
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to issue the UIA to a banned account with the Bob's key
         // This should fail because Bob is not authorized to issue NATHANCOIN to banned account (banned3)
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin.id), banned3.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,1],[2,"predicate_was_false"]
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to issue the UIA to an allowed account
         // This should succeed because Bob is authorized to issue NATHANCOIN to any account
         //////
         issue_op = issue_amount_to(nathan.get_id(), asset(100, nathancoin.id), allowed3.get_id());
         trx.clear();
         trx.operations = {issue_op};
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of in (in) restriction on an operation field
    * Test of CAA for override_transfer_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to override transfer an asset (NATHANCOIN) from only 2 accounts (suspicious1, suspicious2)
    */
   BOOST_AUTO_TEST_CASE(authorized_override_transfer) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(allowed1)(allowed2)(suspicious1)(suspicious2)(allowed3)(arbitrator));
         fund(nathan, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         // Lambda for issuing an asset to an account
         auto issue_amount_to = [&](const account_id_type &issuer, const asset &amount, const account_id_type &to) {
            asset_issue_operation op;
            op.issuer = issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = to;

            return op;
         };

         // Lambda for reserving an asset from an account
         auto create_override = [&](const account_id_type &issuer, const account_id_type &from, const asset &amount,
                                      const account_id_type &to) {
            override_transfer_operation op;
            op.issuer = issuer;
            op.from = from;
            op.amount = amount;
            op.to = to;

            return op;
         };

         //////
         // Initialize: Create user-issued assets
         //////
         upgrade_to_lifetime_member(nathan);
         create_user_issued_asset("NATHANCOIN", nathan, DEFAULT_UIA_ASSET_ISSUER_PERMISSION);
         create_user_issued_asset( "SPECIALCOIN", nathan,  DEFAULT_UIA_ASSET_ISSUER_PERMISSION);
         generate_blocks(1);
         const asset_object &nathancoin = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("NATHANCOIN");
         const asset_object &specialcoin
                 = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("SPECIALCOIN");

         //////
         // Initialize: Nathan issues her two coins to different accounts
         //////
         asset_issue_operation issue_nathan_to_allowed1_op
                 = issue_amount_to(nathan.get_id(), asset(100, nathancoin.id), allowed1.get_id());
         asset_issue_operation issue_nathan_to_allowed2_op
                 = issue_amount_to(nathan.get_id(), asset(200, nathancoin.id), allowed2.get_id());
         asset_issue_operation issue_nathan_to_allowed3_op
                 = issue_amount_to(nathan.get_id(), asset(300, nathancoin.id), allowed3.get_id());
         asset_issue_operation issue_nathan_to_suspicious1_op
                 = issue_amount_to(nathan.get_id(), asset(100, nathancoin.id), suspicious1.get_id());
         asset_issue_operation issue_nathan_to_suspicious2_op
                 = issue_amount_to(nathan.get_id(), asset(200, nathancoin.id), suspicious2.get_id());

         asset_issue_operation issue_special_to_allowed1_op
                 = issue_amount_to(nathan.get_id(), asset(1000, specialcoin.id), allowed1.get_id());
         asset_issue_operation issue_special_to_allowed2_op
                 = issue_amount_to(nathan.get_id(), asset(2000, specialcoin.id), allowed2.get_id());
         asset_issue_operation issue_special_to_allowed3_op
                 = issue_amount_to(nathan.get_id(), asset(3000, specialcoin.id), allowed3.get_id());
         asset_issue_operation issue_special_to_suspicious1_op
                 = issue_amount_to(nathan.get_id(), asset(1000, specialcoin.id), suspicious1.get_id());
         asset_issue_operation issue_special_to_suspicious2_op
                 = issue_amount_to(nathan.get_id(), asset(2000, specialcoin.id), suspicious2.get_id());
         trx.clear();
         trx.operations = {issue_nathan_to_allowed1_op, issue_nathan_to_allowed2_op, issue_nathan_to_allowed3_op,
                 issue_nathan_to_suspicious1_op, issue_nathan_to_suspicious2_op,
                 issue_special_to_allowed1_op, issue_special_to_allowed2_op, issue_special_to_allowed3_op,
                 issue_special_to_suspicious1_op, issue_special_to_suspicious2_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Nathan attempts to override some NATHANCOIN from some account
         // This should succeed because Nathan is the issuer
         //////
         override_transfer_operation override_op
                 = create_override(nathan.get_id(), allowed1.get_id(), asset(20, nathancoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);
         int64_t allowed1_balance_nathancoin_after_override1 = get_balance(allowed1.get_id(), nathancoin.get_id());
         BOOST_CHECK_EQUAL(allowed1_balance_nathancoin_after_override1, 80);

         override_op
                 = create_override(nathan.get_id(), suspicious1.get_id(), asset(20, nathancoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);
         int64_t suspicious1_balance_nathancoin_after_override1
                 = get_balance(suspicious1.get_id(), nathancoin.get_id());
         BOOST_CHECK_EQUAL(suspicious1_balance_nathancoin_after_override1, 80);

         override_op
                 = create_override(nathan.get_id(), allowed1.get_id(), asset(200, specialcoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);
         int64_t allowed1_balance_specialcoin_after_override1 = get_balance(allowed1.get_id(), specialcoin.id);
         BOOST_CHECK_EQUAL(allowed1_balance_specialcoin_after_override1, 800);

         override_op
                 = create_override(nathan.get_id(), suspicious1.get_id(), asset(200, specialcoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);
         int64_t suspicious1_balance_specialcoin_after_override1 = get_balance(suspicious1.get_id(), specialcoin.id);
         BOOST_CHECK_EQUAL(suspicious1_balance_specialcoin_after_override1, 800);


         //////
         // Bob attempts to override some NATHANCOIN and SPECIAL from some accounts
         // This should fail because Bob is not authorized to override any NATHANCOIN nor SPECIALCOIN
         //////
         override_op = create_override(nathan.get_id(), allowed1.get_id(), asset(25, nathancoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

         override_op = create_override(nathan.get_id(), allowed1.get_id(), asset(25, specialcoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});


         //////
         // Nathan authorizes Bob to override transfer NATHANCOIN on its behalf
         // only for accounts suspicious1, and suspicious2
         //////
         custom_authority_create_operation authorize_to_override;
         authorize_to_override.account = nathan.get_id();
         authorize_to_override.auth.add_authority(bob.get_id(), 1);
         authorize_to_override.auth.weight_threshold = 1;
         authorize_to_override.enabled = true;
         authorize_to_override.valid_to = db.head_block_time() + 1000;
         authorize_to_override.operation_type = operation::tag<override_transfer_operation>::value;

         auto amount_index = member_index<override_transfer_operation>("amount");
         auto asset_id_index = member_index<asset>("asset_id");
         authorize_to_override.restrictions
                 .emplace_back(restriction(amount_index, restriction::func_attr, vector<restriction>{
                 restriction(asset_id_index, restriction::func_eq, nathancoin.get_id())}));
         auto from_index = member_index<override_transfer_operation>("from");
         authorize_to_override.restrictions
                 .emplace_back(from_index, FUNC(in),
                               flat_set<account_id_type>{suspicious1.get_id(), suspicious2.get_id()});
         //[
         //  {
         //    "member_index": 4,
         //    "restriction_type": 10,
         //    "argument": [
         //      39,
         //      [
         //        {
         //          "member_index": 1,
         //          "restriction_type": 0,
         //          "argument": [
         //            8,
         //            "1.3.2"
         //          ],
         //          "extensions": []
         //        }
         //      ]
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 2,
         //    "restriction_type": 6,
         //    "argument": [
         //      26,
         //      [
         //        "1.2.20",
         //        "1.2.21"
         //      ]
         //    ],
         //    "extensions": []
         //  }
         //]

         trx.clear();
         trx.operations = {authorize_to_override};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate a distinctive hash ID for the reused operation
         //////
         generate_blocks(1);


         //////
         // Bob attempts to override transfer some NATHANCOIN from a suspicious account
         // This should succeed because Bob is now authorized to override NATHANCOIN from some accounts
         //////
         override_op = create_override(nathan.get_id(), suspicious1.get_id(), asset(25, nathancoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);
         int64_t suspicious1_balance_nathancoin_after_override2
                 = get_balance(suspicious1.get_id(), nathancoin.get_id());
         BOOST_CHECK_EQUAL(suspicious1_balance_nathancoin_after_override2, suspicious1_balance_nathancoin_after_override1 - 25);


         //////
         // Bob attempts to override transfer some SPECIALCOIN from a suspicious account
         // This should fail because Bob is not authorized to override SPECIALCOIN from any accounts
         //////
         override_op = create_override(nathan.get_id(), suspicious1.get_id(), asset(250, specialcoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,0],[0,0],[2,"predicate_was_false"]
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to override transfer some SPECIALCOIN from an allowed account
         // This should fail because Bob is not authorized to override SPECIALCOIN from any accounts
         //////
         override_op = create_override(nathan.get_id(), allowed3.get_id(), asset(250, specialcoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,0],[0,0],[2,"predicate_was_false"]
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to override transfer some NATHANCOIN from an allowed account
         // This should fail because Bob is only authorized to override NATHANCOIN from suspicious accounts
         //////
         override_op = create_override(nathan.get_id(), allowed2.get_id(), asset(20, nathancoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should indicate the rejection path
         // "rejection_path":[[0,1],[2,"predicate_was_false"]
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         int64_t allowed2_balance_nathancoin_after_no_override
                 = get_balance(allowed2.get_id(), nathancoin.get_id());
         BOOST_CHECK_EQUAL(allowed2_balance_nathancoin_after_no_override, 200);
         int64_t allowed2_balance_specialcoin_no_override
                 = get_balance(allowed2.get_id(), specialcoin.get_id());
         BOOST_CHECK_EQUAL(allowed2_balance_specialcoin_no_override, 2000);


         //////
         // Nathan attempts to override transfer of SPECIAL COIN from an allowed account
         // This should succeed because Nathan has not revoked her own authorities as issuer
         //////
         override_op = create_override(nathan.get_id(), allowed3.get_id(), asset(500, specialcoin.id), arbitrator.get_id());
         trx.clear();
         trx.operations = {override_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);
         int64_t allowed3_balance_nathancoin_after_no_override
                 = get_balance(allowed3.get_id(), nathancoin.get_id());
         BOOST_CHECK_EQUAL(allowed3_balance_nathancoin_after_no_override, 300);
         int64_t allowed3_balance_specialcoin_after_override1
                 = get_balance(allowed3.get_id(), specialcoin.get_id());
         BOOST_CHECK_EQUAL(allowed3_balance_specialcoin_after_override1, 3000 - 500);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of a restriction on an optional operation field
    * Variation of the the original transfer_with_memo test for CAA
    * Bob is authorized to transfer Nathan's account to Charlies's account if
    * - the memo is not set OR
    * - the memo is set where the "from" equal's Bob's public key and "to" equals Diana's public *active* key
    * (The active key is chosen for simplicity. Other keys such as the memo key or an alternate key could also be used.)
    */
   BOOST_AUTO_TEST_CASE(authorized_transfer_with_memo_1) {
      try {
         //////
         // Initialize the test
         //////
         ACTORS((nathan)(bob)(charlie)(diana))
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object& gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);

         transfer(account_id_type(), nathan_id, asset(1000));
         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 1000);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 00);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);


         //////
         // Nathan transfers to Charlie with her own authorization
         //////
         transfer_operation top;
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         top.memo = memo_data();
         top.memo->set_message(nathan_private_key, bob_public_key, "Dear Bob,\n\nMoney!\n\nPeace, Nathan");
         trx.operations = {top};
         trx.sign(nathan_private_key, db.get_chain_id());
         auto processed = PUSH_TX(db, trx);

         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 950);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 50);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);

         auto memo = db.get_recent_transaction(processed.id()).operations.front().get<transfer_operation>().memo;
         BOOST_CHECK(memo);
         BOOST_CHECK_EQUAL(memo->get_message(bob_private_key, nathan_public_key), "Dear Bob,\n\nMoney!\n\nPeace, Nathan");


         //////
         // Bob attempts to transfer from Nathan to Charlie
         // This should fail because Bob is not authorized
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the re-used transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});


         //////
         // Nathan authorizes Bob to transfer to Charlie if
         // - the memo is not set OR
         // - the memo is set where the "from" equal's Bob's public key and "to" equals Diana's public key
         //////
         custom_authority_create_operation caop;
         caop.account = nathan.get_id();
         caop.auth.add_authority(bob.get_id(), 1);
         caop.auth.weight_threshold = 1;
         caop.enabled = true;
         caop.valid_to = db.head_block_time() + 1000;
         caop.operation_type = operation::tag<transfer_operation>::value;

         vector<restriction> restrictions;

         // Restriction 1 should have "to" to equal Charlie
         auto to_index = member_index<transfer_operation>("to");
         auto memo_index = member_index<transfer_operation>("memo");
         auto to_inside_memo_index = member_index<memo_data>("to");
         restrictions.emplace_back(to_index, FUNC(eq), charlie.get_id());

         // Restriction 2 is logical OR restriction
         // Branch 1 should have the memo "to" to not be set (to equal void)
         vector<restriction> branch1 = vector<restriction>{restriction(memo_index, FUNC(eq), void_t())};
         // Branch 2 should have the memo "to" reference Diana's public *active* key
         // and "from" reference Bob's public *active* key
         auto from_inside_memo_index = member_index<memo_data>("from");
         vector<restriction> branch2 = vector<restriction>{restriction(memo_index, restriction::func_attr,
                                                                       vector<restriction>{
                 restriction(to_inside_memo_index, FUNC(eq), diana_public_key),
                 restriction(from_inside_memo_index, FUNC(eq), bob_public_key)})};
         unsigned_int dummy_index = 999;
         restriction or_restriction = restriction(dummy_index, FUNC(logical_or), vector<vector<restriction>>{branch1, branch2});
         restrictions.emplace_back(or_restriction);
         caop.restrictions = restrictions;
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 0,
         //    "argument": [
         //      7,
         //      "1.2.18"
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 999,
         //    "restriction_type": 11,
         //    "argument": [
         //      40,
         //      [
         //        [
         //          {
         //            "member_index": 4,
         //            "restriction_type": 0,
         //            "argument": [
         //              0,
         //              {}
         //            ],
         //            "extensions": []
         //          }
         //        ],
         //        [
         //          {
         //            "member_index": 4,
         //            "restriction_type": 10,
         //            "argument": [
         //              39,
         //              [
         //                {
         //                  "member_index": 1,
         //                  "restriction_type": 0,
         //                  "argument": [
         //                    5,
         //                    "RVP6MWg7PpE6azCGwKuhB17DbtSqhzf8i25hspdhndsf7VfsLee7k"
         //                  ],
         //                  "extensions": []
         //                },
         //                {
         //                  "member_index": 0,
         //                  "restriction_type": 0,
         //                  "argument": [
         //                    5,
         //                    "RVP5VE6Dgy9FUmd1mFotXwF88HkQN1KysCWLPqpVnDMjRvGRi1YrM"
         //                  ],
         //                  "extensions": []
         //                }
         //              ]
         //            ],
         //            "extensions": []
         //          }
         //        ]
         //      ]
         //    ],
         //    "extensions": []
         //  }
         //]

         trx.clear();
         trx.operations = {caop};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);

         //////
         // Bob attempts to transfer from Nathan to Charlie WITHOUT a memo
         // This should succeed
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the re-used transfer op
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);

         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 900);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 100);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);

         //////
         // Bob attempts to transfer from Nathan to Charlie with a memo
         // where "from" equals Bob's public key and "to" equals Diana's public key
         // This should succeed
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the similar transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         top.memo = memo_data();
         top.memo->set_message(bob_private_key, diana_public_key,
                               "Dear Diana,\n\nOnly you should be able to read this\n\nLove, Bob");
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         processed = PUSH_TX(db, trx);

         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 850);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 150);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);

         memo = db.get_recent_transaction(processed.id()).operations.front().get<transfer_operation>().memo;
         BOOST_CHECK(memo);
         BOOST_CHECK_EQUAL(memo->get_message(diana_private_key, bob_public_key),
                           "Dear Diana,\n\nOnly you should be able to read this\n\nLove, Bob");

         //////
         // Bob attempts to transfer from Nathan to Charlie with a memo
         // where "from" equals Bob's public key and "to" equals Charlie's public key
         // This should fail because it violates the memo restriction
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the similar transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         top.memo = memo_data();
         top.memo->set_message(bob_private_key, charlie_public_key,
                               "Dear Charlie,\n\nOnly you should be able to read this\n\nLove, Bob");
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);

         // The failure should indicate a violation of both branches of the OR memo restrictions
         // JSON style check of the rejection path
         // JSON-formatted Rejection path
         //[ // A vector of predicate results
         //  [
         //    0, // Index 0 (the outer-most) rejection path
         //    1  // 1 is the index for Restriction 2
         //  ],
         //  [
         //    1, // A (sub-)vector of predicate results
         //    [
         //      {
         //        "success": false,
         //        "rejection_path": [
         //          [
         //            0, // Index 0 of Branch 1 rejection path
         //            0  // Restriction 1 along this branch
         //          ],
         //          [
         //            2, // Rejection reason
         //            "predicate_was_false"
         //          ]
         //        ]
         //      },
         //      {
         //        "success": false,
         //        "rejection_path": [
         //          [
         //            0, // Index 0 of Branch 2 rejection path
         //            0  // Restriction 1 along this branch
         //          ],
         //          [
         //            0, // Index 1 of Branch 2 rejection path
         //            0  // First and only attribute of sub-restriction
         //          ],
         //          [
         //            2, // Rejection reeason
         //            "predicate_was_false"
         //          ]
         //        ]
         //      }
         //    ]
         //  ]
         //]
         EXPECT_EXCEPTION_STRING("[[0,1],[1,[{\"success\":false,\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]},{\"success\":false,\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]]}]]]", [&] {PUSH_TX(db, trx);});

         //////
         // Bob attempts to transfer from Nathan to Diana
         // This should fail because the transfer must be to Charlie
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the similar transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = diana.get_id();
         top.amount = asset(50);
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of a restriction on an optional operation field
    * Variation of the the original transfer_with_memo test for CAA
    * Bob is authorized to transfer from Nathan's account to Charlies's account only if
    * - the memo is set where the "from" equal's Bob's public key and "to" equals Diana's public *active* key
    * (The active key is chosen for simplicity. Other keys such as the memo key or an alternate key could also be used.)
    *
    * A memo field is implicitly required.  Attempts without a memo field should have a rejection reason of null_optional
    */
   BOOST_AUTO_TEST_CASE(authorized_transfer_with_memo_2) {
      try {
         //////
         // Initialize the test
         //////
         ACTORS((nathan)(bob)(charlie)(diana))
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object& gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);

         transfer(account_id_type(), nathan_id, asset(1000));
         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 1000);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 00);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);


         //////
         // Nathan transfers to Charlie with her own authorization
         //////
         transfer_operation top;
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         top.memo = memo_data();
         top.memo->set_message(nathan_private_key, bob_public_key, "Dear Bob,\n\nMoney!\n\nPeace, Nathan");
         trx.operations = {top};
         trx.sign(nathan_private_key, db.get_chain_id());
         auto processed = PUSH_TX(db, trx);

         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 950);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 50);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);

         auto memo = db.get_recent_transaction(processed.id()).operations.front().get<transfer_operation>().memo;
         BOOST_CHECK(memo);
         BOOST_CHECK_EQUAL(memo->get_message(bob_private_key, nathan_public_key), "Dear Bob,\n\nMoney!\n\nPeace, Nathan");


         //////
         // Bob attempts to transfer from Nathan to Charlie
         // This should fail because Bob is not authorized
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the re-used transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
         // "rejected_custom_auths":[]
         EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});


         //////
         // Nathan authorizes Bob to transfer to Charlie if
         // - the memo is set where the "from" equal's Bob's public key and "to" equals Diana's public key
         //////
         custom_authority_create_operation caop;
         caop.account = nathan.get_id();
         caop.auth.add_authority(bob.get_id(), 1);
         caop.auth.weight_threshold = 1;
         caop.enabled = true;
         caop.valid_to = db.head_block_time() + 1000;
         caop.operation_type = operation::tag<transfer_operation>::value;

         vector<restriction> restrictions;

         // Restriction 1 should have "to" to equal Charlie
         auto to_index = member_index<transfer_operation>("to");
         auto memo_index = member_index<transfer_operation>("memo");
         auto to_inside_memo_index = member_index<memo_data>("to");
         restrictions.emplace_back(to_index, FUNC(eq), charlie.get_id());

         // Branch 2 should have the memo "to" reference Diana's public *active* key
         // and "from" reference Bob's public *active* key
         auto from_inside_memo_index = member_index<memo_data>("from");
         restrictions.emplace_back(restriction(memo_index, restriction::func_attr,
                                               vector<restriction>{
                                                       restriction(to_inside_memo_index, FUNC(eq), diana_public_key),
                                                       restriction(from_inside_memo_index, FUNC(eq), bob_public_key)}));
         caop.restrictions = restrictions;
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 0,
         //    "argument": [
         //      7,
         //      "1.2.18"
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 4,
         //    "restriction_type": 10,
         //    "argument": [
         //      39,
         //      [
         //        {
         //          "member_index": 1,
         //          "restriction_type": 0,
         //          "argument": [
         //            5,
         //            "RVP6MWg7PpE6azCGwKuhB17DbtSqhzf8i25hspdhndsf7VfsLee7k"
         //          ],
         //          "extensions": []
         //        },
         //        {
         //          "member_index": 0,
         //          "restriction_type": 0,
         //          "argument": [
         //            5,
         //            "RVP5VE6Dgy9FUmd1mFotXwF88HkQN1KysCWLPqpVnDMjRvGRi1YrM"
         //          ],
         //          "extensions": []
         //        }
         //      ]
         //    ],
         //    "extensions": []
         //  }
         //]

         trx.clear();
         trx.operations = {caop};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Bob attempts to transfer from Nathan to Charlie WITHOUT a memo
         // This should fail because Restriction 2 expects a memo
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the re-used transfer op
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [2,"null_optional"]: 0 is the rejection_indicator for rejection_reason; "null_optional" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"null_optional\"]]", [&] {PUSH_TX(db, trx);});


         //////
         // Bob attempts to transfer from Nathan to Charlie with a memo
         // where "from" equals Bob's public key and "to" equals Diana's public key
         // This should succeed
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the similar transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         top.memo = memo_data();
         top.memo->set_message(bob_private_key, diana_public_key,
                               "Dear Diana,\n\nOnly you should be able to read this\n\nLove, Bob");
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         processed = PUSH_TX(db, trx);

         BOOST_CHECK_EQUAL(get_balance(nathan_id, asset_id_type()), 900);
         BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 0);
         BOOST_CHECK_EQUAL(get_balance(charlie_id, asset_id_type()), 100);
         BOOST_CHECK_EQUAL(get_balance(diana_id, asset_id_type()), 0);

         memo = db.get_recent_transaction(processed.id()).operations.front().get<transfer_operation>().memo;
         BOOST_CHECK(memo);
         BOOST_CHECK_EQUAL(memo->get_message(diana_private_key, bob_public_key),
                           "Dear Diana,\n\nOnly you should be able to read this\n\nLove, Bob");

         //////
         // Bob attempts to transfer from Nathan to Charlie with a memo
         // where "from" equals Bob's public key and "to" equals Charlie's public key
         // This should fail because it violates the memo restriction
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the similar transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = charlie.get_id();
         top.amount = asset(50);
         top.memo = memo_data();
         top.memo->set_message(bob_private_key, charlie_public_key,
                               "Dear Charlie,\n\nOnly you should be able to read this\n\nLove, Bob");
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "null_optional" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

         //////
         // Bob attempts to transfer from Nathan to Diana
         // This should fail because transfer must be to Charlie
         //////
         generate_blocks(1); // Advance the blockchain to generate a distinctive hash ID for the similar transfer op
         top = transfer_operation();
         top.from = nathan.get_id();
         top.to = diana.get_id();
         top.amount = asset(50);
         trx.clear();
         trx.operations = {top};
         sign(trx, bob_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
         // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index for Restriction 1
         // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
         EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Generate a random pre-image for HTLC-related tests
    */
   void generate_random_preimage(uint16_t key_size, std::vector<char>& vec)
   {
      std::independent_bits_engine<std::default_random_engine, sizeof(unsigned), unsigned int> rbe;
      std::generate(begin(vec), end(vec), std::ref(rbe));
      return;
   }


   /**
    * Test of greater than or equal to (ge) restriction on a field
    * Test of CAA for htlc_create_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to create an HTLC operation as long as the pre-image size is greater than or equal to a specified size
    *
    * This test is similar to the HTLC test called "other_peoples_money"
    */
   BOOST_AUTO_TEST_CASE(authorized_htlc_creation) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         set_expiration(db, trx);

         // Initialize HTLC blockchain parameters
         trx.clear();
         set_htlc_committee_parameters();
         generate_blocks(5);

         // Initialize CAA blockchain parameters
         trx.clear();
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(gateway));
         int64_t init_balance(100 * GRAPHENE_BLOCKCHAIN_PRECISION );
         transfer( committee_account, nathan_id, graphene::chain::asset(init_balance) );


         //////
         // Initialize: Pre-image sizes and pre-images to reduce the test variability
         //////
         uint16_t pre_image_size_256 = 256;
         std::vector<char> pre_image_256(pre_image_size_256);
         generate_random_preimage(pre_image_size_256, pre_image_256);

         // The minimum pre-image size that will be authorized by Nathan
         uint16_t authorized_minimum_pre_image_size_512 = 512;

         int64_t pre_image_size_512 = int64_t(authorized_minimum_pre_image_size_512 + 0);
         std::vector<char> pre_image_512(pre_image_size_512);
         generate_random_preimage(pre_image_size_512, pre_image_512);

         int64_t pre_image_size_600 = int64_t(authorized_minimum_pre_image_size_512 + 88);
         std::vector<char> pre_image_600(pre_image_size_600);
         generate_random_preimage(pre_image_size_600, pre_image_600);


         //////
         // Nathan attempts to put a contract on the blockchain using Nathan's funds
         // This should succeed because Nathan is authorized to create HTLC for her own account
         //////
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PRECISION );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 3;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_256 );
            create_operation.preimage_size = pre_image_size_256;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, nathan_private_key);
            PUSH_TX( db, trx );
         }


         //////
         // Advance the blockchain to generate distinctive hash IDs for the similar transactions
         //////
         generate_blocks(1);


         //////
         // Bob attempts to put a contract on the blockchain using Nathan's funds
         // This should fail because Bob is not authorized to create HTLC on behalf of Nathan
         //////
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PRECISION );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 3;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_256 );
            create_operation.preimage_size = pre_image_size_256;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Nathan authorizes Bob to create HTLC only to an account (gateway)
         // and if the pre-image size is greater than or equal to 512 bytes
         //////
         custom_authority_create_operation authorize_htlc_create;
         authorize_htlc_create.account = nathan.get_id();
         authorize_htlc_create.auth.add_authority(bob.get_id(), 1);
         authorize_htlc_create.auth.weight_threshold = 1;
         authorize_htlc_create.enabled = true;
         authorize_htlc_create.valid_to = db.head_block_time() + 1000;
         authorize_htlc_create.operation_type = operation::tag<htlc_create_operation>::value;

         auto to_index = member_index<htlc_create_operation>("to");
         authorize_htlc_create.restrictions.emplace_back(to_index, FUNC(eq), gateway.get_id());

         auto preimage_size_index = member_index<htlc_create_operation>("preimage_size");
         authorize_htlc_create.restrictions.emplace_back(restriction(preimage_size_index, FUNC(ge), pre_image_size_512));
         //[
         //  {
         //    "member_index": 2,
         //    "restriction_type": 0,
         //    "argument": [
         //      7,
         //      "1.2.18"
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 5,
         //    "restriction_type": 5,
         //    "argument": [
         //      2,
         //      512
         //    ],
         //    "extensions": []
         //  }
         //]
         trx.clear();
         trx.operations = {authorize_htlc_create};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate distinctive hash IDs for the similar transactions
         //////
         generate_blocks(1);


         //////
         // Bob attempts to put a contract on the blockchain using Nathan's funds
         // with a preimage size of 256.
         // This should fail because Bob is not authorized to create HTLC on behalf of Nathan
         // if the preimage size is below the minimum value restriction.
         //////
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PRECISION );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 3;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_256 );
            create_operation.preimage_size = pre_image_size_256;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,1],[2,"predicate_was_false"]
            // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

         }

         //////
         // Bob attempts to put a contract on the blockchain using Nathan's funds
         // with a preimage size of 512.
         // This should succeed because Bob is authorized to create HTLC on behalf of Nathan
         // and the preimage size equals the minimum value restriction.
         //////
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PRECISION );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 3;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_512 );
            create_operation.preimage_size = pre_image_size_512;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, bob_private_key);
            PUSH_TX( db, trx );

         }


         //////
         // Bob attempts to put a contract on the blockchain using Nathan's funds
         // with a preimage size of 600.
         // This should succeed because Bob is authorized to create HTLC on behalf of Nathan
         // and the preimage size is greater than the minimum value restriction.
         //////
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PRECISION );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 3;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_600 );
            create_operation.preimage_size = pre_image_size_600;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, bob_private_key);
            PUSH_TX( db, trx );

         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of vector field size comparison
    * Test of CAA for htlc_redeem_operation
    *
    * Scenario: Test of authorization of one account (gateway) authorizing another account (bob)
    * to redeem an HTLC operation
    */
   BOOST_AUTO_TEST_CASE(authorized_htlc_redeem) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         set_expiration(db, trx);

         // Initialize HTLC blockchain parameters
         trx.clear();
         set_htlc_committee_parameters();
         generate_blocks(5);

         // Initialize CAA blockchain parameters
         trx.clear();
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });

         // Update the expiration of the re-usable trx relative to the head block time
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(gateway));
         int64_t init_balance(1000 * GRAPHENE_BLOCKCHAIN_PRECISION );
         transfer( committee_account, nathan_id, graphene::chain::asset(init_balance) );
         int64_t init_gateway_balance(50 * GRAPHENE_BLOCKCHAIN_PRECISION);
         transfer( committee_account, gateway_id, graphene::chain::asset(init_gateway_balance) );


         //////
         // Initialize: Pre-image sizes and pre-images to reduce the test variability
         //////
         uint16_t pre_image_size_256 = 256;
         std::vector<char> pre_image_256(pre_image_size_256);
         generate_random_preimage(pre_image_size_256, pre_image_256);


         //////
         // Gateway puts a contract on the blockchain
         // This should succeed because the gateway is authorized to create HTLC for its own account
         //////
         share_type htlc_amount = 25 * GRAPHENE_BLOCKCHAIN_PRECISION;
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( htlc_amount );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 86400;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_256 );
            create_operation.preimage_size = pre_image_size_256;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, nathan_private_key);
            PUSH_TX( db, trx );
         }


         //////
         // Advance the blockchain to get the finalized HTLC ID
         //////
         generate_blocks(1);
         graphene::chain::htlc_id_type nathan_htlc_id =
                 db.get_index_type<htlc_index>().indices().get<by_from_id>().find(nathan.get_id())->id;


         //////
         // Bob attempts to redeem the HTLC on behalf of the gateway
         // This should fail because Bob is not authorized to redeem on behalf of the gateway
         //////
         graphene::chain::htlc_redeem_operation redeem_operation;
         {
            redeem_operation.redeemer = gateway_id;
            redeem_operation.htlc_id = nathan_htlc_id;
            redeem_operation.preimage = pre_image_256;
            redeem_operation.fee = db.current_fee_schedule().calculate_fee( redeem_operation );
            trx.clear();
            trx.operations.push_back(redeem_operation);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Gateway authorizes Bob to redeem an HTLC
         // only if the preimage length equals 200 bytes
         // This length is incompatible with the HTLC pre-image that is already on the blockchain
         //////
         custom_authority_create_operation authorize_htlc_redeem;
         authorize_htlc_redeem.account = gateway.get_id();
         authorize_htlc_redeem.auth.add_authority(bob.get_id(), 1);
         authorize_htlc_redeem.auth.weight_threshold = 1;
         authorize_htlc_redeem.enabled = true;
         authorize_htlc_redeem.valid_to = db.head_block_time() + 1000;
         authorize_htlc_redeem.operation_type = operation::tag<htlc_redeem_operation>::value;

         auto preimage_index = member_index<htlc_redeem_operation>("preimage");
         authorize_htlc_redeem.restrictions.emplace_back(preimage_index, FUNC(eq), int64_t(200));
         //[
         //  {
         //    "member_index": 3,
         //    "restriction_type": 0,
         //    "argument": [
         //      2,
         //      200
         //    ],
         //    "extensions": []
         //  }
         //]

         trx.clear();
         trx.operations = {authorize_htlc_redeem};
         sign(trx, gateway_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to get the finalized CAA ID
         //////
         generate_blocks(1);
         auto caa = db.get_index_type<custom_authority_index>().indices().get<by_account_custom>().find(gateway.get_id());
         custom_authority_id_type caa_id = caa->id;


         //////
         // Bob attempts to redeem the HTLC
         // This should fail because the authorization's restriction prohibits the redemption of this HTLC
         //////
         {
            trx.clear();
            trx.operations.push_back(redeem_operation);
            sign(trx, bob_private_key);

            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Advance the blockchain to generate distinctive hash IDs for the similar transactions
         //////
         generate_blocks(1);


         //////
         // Gateway updates the authorization for to redeem an HTLC
         // only if the preimage length equals 256 bytes
         // This length is compatible with the HTLC pre-image that is already on the blockchain
         //////
         custom_authority_update_operation update_authorization;
         update_authorization.account = gateway.get_id();
         update_authorization.authority_to_update = caa_id;
         uint16_t existing_restriction_index = 0; // The 0-based index of the first and only existing restriction
         update_authorization.restrictions_to_remove = {existing_restriction_index};
         update_authorization.restrictions_to_add =
                 {restriction(preimage_index, FUNC(eq), int64_t(pre_image_size_256))};
         trx.clear();
         trx.operations = {update_authorization};
         sign(trx, gateway_private_key);
         PUSH_TX(db, trx);


         //////
         // Bob attempts to redeem the HTLC
         // This should succeed because the redemption satisfies the authorization
         //////
         {
            trx.clear();
            trx.operations.push_back(redeem_operation);
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);

         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of greater than (gt) and less than or equal to (le) restriction on a field
    * Test of CAA for htlc_extend_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to extend an HTLC operation as long as the extension is within a specified duration
    */
   BOOST_AUTO_TEST_CASE(authorized_htlc_extension) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         set_expiration(db, trx);

         // Initialize HTLC blockchain parameters
         trx.clear();
         set_htlc_committee_parameters();
         generate_blocks(5);

         // Initialize CAA blockchain parameters
         trx.clear();
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(gateway));
         int64_t init_balance(1000 * GRAPHENE_BLOCKCHAIN_PRECISION );
         transfer( committee_account, nathan_id, graphene::chain::asset(init_balance) );
         int64_t init_gateway_balance(50 * GRAPHENE_BLOCKCHAIN_PRECISION);
         transfer( committee_account, gateway_id, graphene::chain::asset(init_gateway_balance) );


         //////
         // Initialize: Pre-image sizes and pre-images to reduce the test variability
         //////
         uint16_t pre_image_size_256 = 256;
         std::vector<char> pre_image_256(pre_image_size_256);
         generate_random_preimage(pre_image_size_256, pre_image_256);


         //////
         // Gateway puts a contract on the blockchain
         // This should succeed because the gateway is authorized to create HTLC for its own account
         //////
         share_type htlc_amount = 25 * GRAPHENE_BLOCKCHAIN_PRECISION;
         {
            graphene::chain::htlc_create_operation create_operation;
            create_operation.amount = graphene::chain::asset( htlc_amount );
            create_operation.from = nathan_id;
            create_operation.to = gateway_id;
            create_operation.claim_period_seconds = 86400;
            create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image_256 );
            create_operation.preimage_size = pre_image_size_256;
            create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
            trx.clear();
            trx.operations.push_back(create_operation);
            sign(trx, nathan_private_key);
            PUSH_TX( db, trx );
         }


         //////
         // Advance the blockchain to get the finalized HTLC ID
         //////
         generate_blocks(1);
         graphene::chain::htlc_id_type nathan_htlc_id =
                 db.get_index_type<htlc_index>().indices().get<by_from_id>().find(nathan.get_id())->id;


         //////
         // Bob attempts to extend the HTLC
         // This should fail because Bob is not authorized to extend an HTLC on behalf of Nathan
         //////
         graphene::chain::htlc_extend_operation extend_operation;
         {
            extend_operation.update_issuer = nathan_id;
            extend_operation.htlc_id = nathan_htlc_id;
            extend_operation.seconds_to_add = int64_t(24 * 3600);
            extend_operation.fee = db.current_fee_schedule().calculate_fee( extend_operation );
            trx.clear();
            trx.operations.push_back(extend_operation);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Nathan authorizes Bob to extend an HTLC
         // by greater than 1 hour and less than or equal to 24 hours
         //////
         custom_authority_create_operation authorize_htlc_extension;
         authorize_htlc_extension.account = nathan.get_id();
         authorize_htlc_extension.auth.add_authority(bob.get_id(), 1);
         authorize_htlc_extension.auth.weight_threshold = 1;
         authorize_htlc_extension.enabled = true;
         authorize_htlc_extension.valid_to = db.head_block_time() + 1000;
         authorize_htlc_extension.operation_type = operation::tag<htlc_extend_operation>::value;

         // Authorization to extend is restricted to greater than 1 hour and less than or equal to 24 hours
         vector<restriction> restrictions;
         auto extension_duration_index = member_index<htlc_extend_operation>("seconds_to_add");
         // Duration extension greater than one hour
         restriction restriction_gt_duration = restriction(extension_duration_index, FUNC(gt), int64_t(1 * 3600));
         restrictions.emplace_back(restriction_gt_duration);
         // Duration extension less than or equal to 24 hours
         restriction restriction_le_duration = restriction(extension_duration_index, FUNC(le), int64_t(24 * 3600));
         restrictions.emplace_back(restriction_le_duration);
         authorize_htlc_extension.restrictions = restrictions;
         //[
         //  {
         //    "member_index": 3,
         //    "restriction_type": 4,
         //    "argument": [
         //      2,
         //      3600
         //    ],
         //    "extensions": []
         //  },
         //  {
         //    "member_index": 3,
         //    "restriction_type": 3,
         //    "argument": [
         //      2,
         //      86400
         //    ],
         //    "extensions": []
         //  }
         //]
         trx.clear();
         trx.operations = {authorize_htlc_extension};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate distinctive hash IDs for the similar transactions
         //////
         generate_blocks(1);


         //////
         // Bob attempts to extend the HTLC
         // This should succeed because Bob is conditionally authorized to extend
         //////
         {
            trx.clear();
            trx.operations.push_back(extend_operation);
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);

         }


         //////
         // Bob attempts to extend the HTLC by exactly 10 hours
         // This should succeed because Bob is authorized to extend the HTLC
         // if greater than 1 hour and less than or equal to 24 hours
         //////
         {
            extend_operation = htlc_extend_operation();
            extend_operation.update_issuer = nathan_id;
            extend_operation.htlc_id = nathan_htlc_id;
            extend_operation.seconds_to_add = int64_t(10 * 3600);
            extend_operation.fee = db.current_fee_schedule().calculate_fee( extend_operation );
            trx.clear();
            trx.operations.push_back(extend_operation);
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);

         }


         //////
         // Bob attempts to extend the HTLC by exactly 1 hour
         // This should fail because Bob is authorized to extend the HTLC
         // if greater than 1 hour and less than or equal to 24 hours
         //////
         {
            extend_operation = htlc_extend_operation();
            extend_operation.update_issuer = nathan_id;
            extend_operation.htlc_id = nathan_htlc_id;
            extend_operation.seconds_to_add = int64_t(1 * 3600);
            extend_operation.fee = db.current_fee_schedule().calculate_fee( extend_operation );
            trx.clear();
            trx.operations.push_back(extend_operation);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,0],[2,"predicate_was_false"]
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Bob attempts to extend the HTLC by 24 hours plus 1 second
         // This should fail because Bob is authorized to extend the HTLC
         // if greater than 1 hour and less than or equal to 24 hours
         //////
         {
            extend_operation = htlc_extend_operation();
            extend_operation.update_issuer = nathan_id;
            extend_operation.htlc_id = nathan_htlc_id;
            extend_operation.seconds_to_add = int64_t( (24 * 3600) + 1);
            extend_operation.fee = db.current_fee_schedule().calculate_fee( extend_operation );
            trx.clear();
            trx.operations.push_back(extend_operation);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,1],[2,"predicate_was_false"]
            // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});

         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of variant assert (variant_assert) restriction on a field
    * Test of CAA for vesting_balance_create_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to create a coins-day vesting balance with a vesting duration of 800,000 seconds
    */
   BOOST_AUTO_TEST_CASE(authorized_vesting_balance_create) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(charlie));
         fund(nathan, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         //////
         // Bob attempts to create a coins-day vesting balance for Nathan
         // This attempt should fail because Nathan has not authorized Bob to create a vesting balance
         //////
         vesting_balance_create_operation original_vb_op;
         time_point_sec policy_start_time = db.head_block_time() + 86400;
         {
            vesting_balance_create_operation vb_op;
            vb_op.creator = nathan_id;
            vb_op.owner = charlie_id;
            vb_op.amount = graphene::chain::asset(60000);
            vb_op.policy = cdd_vesting_policy_initializer(800000, policy_start_time);
            vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
            trx.clear();
            trx.operations.push_back(vb_op);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

            original_vb_op = vb_op;
         }


         //////
         // Nathan authorizes Bob to create a coins-day vesting balance from her funds
         // only if the vesting duration equals 800,000 seconds
         //////
         custom_authority_create_operation authorize_create_vesting;
         authorize_create_vesting.account = nathan.get_id();
         authorize_create_vesting.auth.add_authority(bob.get_id(), 1);
         authorize_create_vesting.auth.weight_threshold = 1;
         authorize_create_vesting.enabled = true;
         authorize_create_vesting.valid_to = db.head_block_time() + 1000;
         authorize_create_vesting.operation_type = operation::tag<vesting_balance_create_operation>::value;

         // Restrict authorization to a coin-days vesting policy with a vesting duration of 800000 seconds
         auto policy_index = member_index<vesting_balance_create_operation>("policy");
         int64_t policy_tag = vesting_policy_initializer::tag<cdd_vesting_policy_initializer>::value;
         auto vesting_seconds_index = member_index<cdd_vesting_policy_initializer>("vesting_seconds");
         vector<restriction> policy_restrictions = {restriction(vesting_seconds_index, FUNC(eq), int64_t(800000))};
         pair<int64_t, vector<restriction>> policy_argument(policy_tag, policy_restrictions);
         authorize_create_vesting.restrictions = {restriction(policy_index, FUNC(variant_assert), policy_argument)};
         //[
         //  {
         //    "member_index": 4,
         //    "restriction_type": 12,
         //    "argument": [
         //      41,
         //      [
         //        1,
         //        [
         //          {
         //            "member_index": 1,
         //            "restriction_type": 0,
         //            "argument": [
         //              2,
         //              800000
         //            ],
         //            "extensions": []
         //          }
         //        ]
         //      ]
         //    ],
         //    "extensions": []
         //  }
         //]
         trx.clear();
         trx.operations = {authorize_create_vesting};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to generate distinctive hash IDs for the similar transactions
         //////
         generate_blocks(1);


         //////
         // Bob attempts to create a coins-day vesting balance for Nathan with a vesting duration of 86400 seconds
         // This attempt should fail because Nathan has not authorized this duration
         //////
         {
            vesting_balance_create_operation vb_op;
            vb_op.creator = nathan_id;
            vb_op.owner = charlie_id;
            vb_op.amount = graphene::chain::asset(60000);
            vb_op.policy = cdd_vesting_policy_initializer(86400, policy_start_time);
            vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
            trx.clear();
            trx.operations.push_back(vb_op);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,0],[0,0],[2,"predicate_was_false"]
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[0,0],[2,\"predicate_was_false\"]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Bob attempts to create a linear vesting balance for Nathan
         // This attempt should fail because Nathan has not authorized this type of vesting balance creation
         //////
         {
            vesting_balance_create_operation vb_op;
            vb_op.creator = nathan_id;
            vb_op.owner = charlie_id;
            vb_op.amount = graphene::chain::asset(60000);
            linear_vesting_policy_initializer policy;
            policy.begin_timestamp = policy_start_time;
            policy.vesting_cliff_seconds = 800000;
            policy.vesting_duration_seconds = 40000;
            vb_op.policy = policy;
            vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
            trx.clear();
            trx.operations.push_back(vb_op);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,0],[2,"incorrect_variant_type"]
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
            // [2,"incorrect_variant_type"]: 0 is the rejection_indicator for rejection_reason; "incorrect_variant_type" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"incorrect_variant_type\"]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Bob attempts to create a coins-day vesting balance for Nathan with a vesting duration of 800000 seconds
         // This attempt should succeed because Nathan has authorized authorized this type of vesting balance creation
         // with this duration
         //////
         {
            trx.clear();
            trx.operations.push_back(original_vb_op);
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);

         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of time restrictions on CAA
    * Test of CAA for vesting_balance_withdraw_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to withdraw vesting for a limited duration
    */
   BOOST_AUTO_TEST_CASE(authorized_time_restrictions_1) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob)(charlie));
         fund(charlie, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         //////
         // Charlie creates an instant vesting balance for Nathan
         //////
         vesting_balance_create_operation original_vb_op;
         time_point_sec policy_start_time = db.head_block_time() + 86400;
         vesting_balance_create_operation vb_op;
         vb_op.creator = charlie_id;
         vb_op.owner = nathan_id;
         vb_op.amount = graphene::chain::asset(60000);
         vb_op.policy = instant_vesting_policy_initializer();
         vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
         trx.clear();
         trx.operations.push_back(vb_op);
         sign(trx, charlie_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to before withdrawal of vesting balance can start
         //////
         generate_blocks(1);
         set_expiration(db, trx);
         vesting_balance_id_type vesting_balance_id =
                 db.get_index_type<vesting_balance_index>().indices().get<by_account>().find(nathan.get_id())->id;


         //////
         // Bob attempts to withdraw some of the vesting balance on behalf of Nathan
         // This attempt should fail because Nathan has not authorized Bob
         //////
         {
            asset partial_amount = asset(10000);

            vesting_balance_withdraw_operation vb_op;
            vb_op.vesting_balance = vesting_balance_id;
            vb_op.owner = nathan_id;
            vb_op.amount = partial_amount;
            vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
            trx.clear();
            trx.operations.push_back(vb_op);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

         }


         //////
         // Nathan authorizes Bob to withdraw her vesting balance
         //////
         custom_authority_create_operation authorize_create_vesting;
         authorize_create_vesting.account = nathan.get_id();
         authorize_create_vesting.auth.add_authority(bob.get_id(), 1);
         authorize_create_vesting.auth.weight_threshold = 1;
         authorize_create_vesting.enabled = true;
         // Authorization is valid only for 3/5 of the maximum duration of a custom authority
         time_point_sec authorization_end_time = policy_start_time + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 3 / 5);
         time_point_sec authorization_before_end_time = policy_start_time + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 1 / 5);
         authorize_create_vesting.valid_to = authorization_end_time;
         authorize_create_vesting.operation_type = operation::tag<vesting_balance_withdraw_operation>::value;
         trx.clear();
         trx.operations = {authorize_create_vesting};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to before the authorization expires
         //////
         generate_blocks(authorization_before_end_time);
         set_expiration(db, trx);


         //////
         // Bob attempts to withdraw the available vesting balance for Nathan
         // This attempt should succeed because the authorization is active
         //////
         {
            asset partial_amount = asset(10000);

            vesting_balance_withdraw_operation vb_op;
            vb_op.vesting_balance = vesting_balance_id;
            vb_op.owner = nathan_id;
            vb_op.amount = partial_amount;
            vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
            trx.clear();
            trx.operations.push_back(vb_op);
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);

         }


         //////
         // Advance the blockchain to after the authorization expires
         //////
         time_point_sec after_authorization_end_time = authorization_end_time + 86400;
         generate_blocks(after_authorization_end_time);
         set_expiration(db, trx);


         //////
         // Bob attempts to withdraw the available vesting balance for Nathan
         // This attempt should fail because the authorization has expired
         //////
         {
            asset partial_amount = asset(10000);

            vesting_balance_withdraw_operation vb_op;
            vb_op.vesting_balance = vesting_balance_id;
            vb_op.owner = nathan_id;
            vb_op.amount = partial_amount;
            vb_op.fee = db.current_fee_schedule().calculate_fee(vb_op);
            trx.clear();
            trx.operations.push_back(vb_op);
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});

         }

      } FC_LOG_AND_RETHROW()
   }



   /**
    * Test of time restrictions on CAA
    * Test of CAA for asset_reserve_operation
    * Test of CAA in a proposed operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to reserve (burn) an asset only during a specfied timespan
    */
   BOOST_AUTO_TEST_CASE(authorized_time_restrictions_3) {
      try {
         //////
         // Initialize: Accounts
         //////
         ACTORS((assetissuer)(feedproducer)(nathan)(bob)(charlie));
         int64_t init_balance(100 * GRAPHENE_BLOCKCHAIN_PRECISION);
         fund(nathan, asset(init_balance));


         // Lambda for issuing an asset to an account
         auto issue_amount_to = [&](const account_id_type &issuer, const asset &amount, const account_id_type &to) {
            asset_issue_operation op;
            op.issuer = issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = to;

            return op;
         };

         // Lambda for reserving an asset from an account
         auto reserve_asset = [&](const account_id_type &reserver, const asset &amount) {
            asset_reserve_operation op;
            op.payer = reserver;
            op.amount_to_reserve = amount;

            return op;
         };


         //////
         // Initialize: Create user-issued assets
         //////
         upgrade_to_lifetime_member(assetissuer);
         create_user_issued_asset("SPECIALCOIN", nathan,  DEFAULT_UIA_ASSET_ISSUER_PERMISSION);
         generate_blocks(1);
         const asset_object &specialcoin
                 = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("SPECIALCOIN");


         //////
         // Initialize: nathan issues SPECIALCOIN to different accounts
         //////
         asset_issue_operation issue_special_to_nathan_op
                 = issue_amount_to(nathan.get_id(), asset(1000, specialcoin.id), nathan.get_id());
         asset_issue_operation issue_special_to_charlie_op
                 = issue_amount_to(nathan.get_id(), asset(2000, specialcoin.id), charlie.get_id());
         trx.clear();
         trx.operations = {issue_special_to_nathan_op, issue_special_to_charlie_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Nathan reserves some SPECIALCOIN from hes account
         //////
         asset_reserve_operation reserve_op = reserve_asset(nathan.get_id(), asset(200, specialcoin.id));
         trx.clear();
         trx.operations = {reserve_op};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);
         int64_t allowed1_balance_specialcoin_after_override1 = get_balance(nathan.get_id(), specialcoin.id);
         BOOST_CHECK_EQUAL(allowed1_balance_specialcoin_after_override1, 800);


         //////
         // Charlie reserves some SPECIALCOIN from his account
         //////
         reserve_op = reserve_asset(charlie.get_id(), asset(200, specialcoin.id));
         trx.clear();
         trx.operations = {reserve_op};
         sign(trx, charlie_private_key);
         PUSH_TX(db, trx);
         int64_t charlie_balance_specialcoin_after_override1 = get_balance(charlie.get_id(), specialcoin.id);
         BOOST_CHECK_EQUAL(charlie_balance_specialcoin_after_override1, 1800);


         //////
         // Nathan authorizes Bob to reserve her SPECIALCOIN
         // This attempt should fail because the blockchain has not yet been initialized for CAA
         //////
         custom_authority_create_operation authorize_reserve;
         authorize_reserve.account = nathan.get_id();
         authorize_reserve.auth.add_authority(bob.get_id(), 1);
         authorize_reserve.auth.weight_threshold = 1;
         authorize_reserve.enabled = true;
         // Authorization is valid only for 2/5 of the maximum duration of a custom authority
         time_point_sec before_authorization_start_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 1 / 5);
         time_point_sec authorization_start_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 2 / 5);
         time_point_sec authorization_middle_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 3 / 5);
         time_point_sec authorization_end_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 4 / 5);
         time_point_sec after_authorization_end_time = authorization_end_time + 86400;
         authorize_reserve.valid_from = authorization_start_time;
         authorize_reserve.valid_to = authorization_end_time;
         authorize_reserve.operation_type = operation::tag<asset_reserve_operation>::value;
         trx.clear();
         trx.operations = {authorize_reserve};
         sign(trx, nathan_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::assert_exception);


         //////
         // Nathan creates a PROPOSAL to authorize Bob to reserve her SPECIALCOIN
         // This attempt should fail because the blockchain has not yet been initialized for CAA
         //////
         proposal_create_operation proposal;
         proposal.fee_paying_account = nathan.get_id();
         proposal.proposed_ops = {op_wrapper(authorize_reserve)};
         proposal.expiration_time = db.head_block_time() + 86400;
         trx.clear();
         trx.operations = {authorize_reserve};
         sign(trx, nathan_private_key);
         BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::assert_exception);


         //////
         // Initialize the blockchain for CAA
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Nathan creates a PROPOSAL to authorize Bob to reserve her SPECIALCOIN
         // Authorization is valid only for 2/5 of the maximum duration of a custom authority
         // This attempt should succeed because the blockchain is initialized for CAA
         //////
         before_authorization_start_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 1 / 5);
         authorization_start_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 2 / 5);
         authorization_middle_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 3 / 5);
         authorization_end_time =
                 db.head_block_time() + (GRAPHENE_DEFAULT_MAX_CUSTOM_AUTHORITY_LIFETIME_SECONDS * 4 / 5);
         after_authorization_end_time = authorization_end_time + 86400;
         authorize_reserve.valid_from = authorization_start_time;
         authorize_reserve.valid_to = authorization_end_time;

         proposal.fee_paying_account = nathan.get_id();
         proposal.proposed_ops = {op_wrapper(authorize_reserve)};
         proposal.expiration_time = db.head_block_time() + 86400;
         trx.clear();
         trx.operations = {proposal};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to get the finalized proposal ID
         //////
         generate_blocks(1);
         const proposal_object& prop = *db.get_index_type<proposal_index>().indices().begin();
         proposal_id_type proposal_id = prop.id;

         // Nathan approves the proposal
         proposal_update_operation approve_proposal;
         approve_proposal.proposal = proposal_id;
         approve_proposal.fee_paying_account = nathan.get_id();
         approve_proposal.active_approvals_to_add = {nathan.get_id()};
         trx.clear();
         trx.operations = {approve_proposal};
         sign(trx, nathan_private_key);
         PUSH_TX(db, trx);


         //////
         // Advance the blockchain to before the authorization starts
         //////
         generate_blocks(before_authorization_start_time);
         set_expiration(db, trx);


         //////
         // Bob attempts to reserve some of Nathan's SPECIALCOIN
         // This attempt should fail because Bob the Nathan authorization is not yet active
         //////
         {
            asset_reserve_operation reserve_op = reserve_asset(nathan.get_id(), asset(200, specialcoin.id));
            trx.clear();
            trx.operations = {reserve_op};
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because the CAA is not yet active
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // Advance the blockchain to the start of the authorization period
         //////
         generate_blocks(authorization_start_time);
         set_expiration(db, trx);


         //////
         // Bob attempts to update the collateral for Nathan's debt position
         // This should succeed because the authorization is active
         //////
         {
            asset_reserve_operation reserve_op = reserve_asset(nathan.get_id(), asset(200, specialcoin.id));
            trx.clear();
            trx.operations = {reserve_op};
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // Advance the blockchain to the end of the authorization period
         //////
         generate_blocks(authorization_middle_time);
         set_expiration(db, trx);


         //////
         // Bob attempts to update the collateral for Nathan's debt position
         // This should succeed because the authorization is active
         //////
         {
            asset_reserve_operation reserve_op = reserve_asset(nathan.get_id(), asset(200, specialcoin.id));
            trx.clear();
            trx.operations = {reserve_op};
            sign(trx, bob_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // Advance the blockchain to after the authorization expires
         //////
         generate_blocks(after_authorization_end_time);
         set_expiration(db, trx);


         //////
         // Bob attempts to update the collateral for Nathan's debt position
         // This should fail because Bob the authorization has expired
         //////
         {
            asset_reserve_operation reserve_op = reserve_asset(nathan.get_id(), asset(200, specialcoin.id));
            trx.clear();
            trx.operations = {reserve_op};
            sign(trx, bob_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});
         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of string field restriction
    * Test of CAA for asset_create_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing another account (bob)
    * to create an asset with a description that starts with the literal string "ACOIN."
    */
   BOOST_AUTO_TEST_CASE(authorized_asset_creation) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan)(bob));
         fund(nathan, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));
         upgrade_to_lifetime_member(nathan);
         fund(bob, asset(200000 * GRAPHENE_BLOCKCHAIN_PRECISION));


         // Lambda for issuing an asset to an account
         auto create_uia = [&](const string& name,
                                    const account_object& issuer,
                                    uint16_t flags,
                                    const additional_asset_options_t& options = additional_asset_options_t(),
                                    const price& core_exchange_rate = price(asset(1, asset_id_type(1)), asset(1)),
                                    uint8_t precision = 2 /* traditional precision for tests */,
                                    uint16_t market_fee_percent = 0) {

            asset_create_operation op;

            op.issuer = issuer.id;
            op.fee = asset();
            op.symbol = name;
            op.common_options.max_supply = 0;
            op.precision = precision;
            op.common_options.core_exchange_rate = core_exchange_rate;
            op.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
            op.common_options.flags = flags;
            op.common_options.issuer_permissions = flags;
            op.common_options.market_fee_percent = market_fee_percent;
            op.common_options.extensions = std::move(options);

            return op;
         };


         //////
         // Nathan creates a UIA
         //////
         {
            asset_create_operation create_uia_op = create_uia("ACOIN", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // Bob attempts to create a UIA
         // This should fail because Bob is not authorized by Nathan to create any coin with Nathan as the issuer
         //////
         {
            asset_create_operation create_uia_op = create_uia("ACOIN.BOB", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);
            //BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            //EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // Nathan authorizes Bob to create sub-token UIAs below ACOIN
         //////
         {
            custom_authority_create_operation authorize_uia_creation;
            authorize_uia_creation.account = nathan.get_id();
            authorize_uia_creation.auth.add_authority(bob.get_id(), 1);
            authorize_uia_creation.auth.weight_threshold = 1;
            authorize_uia_creation.enabled = true;
            authorize_uia_creation.valid_to = db.head_block_time() + 86400;
            authorize_uia_creation.operation_type = operation::tag<asset_create_operation>::value;

            auto symbol_index = member_index<asset_create_operation>("symbol");
            authorize_uia_creation.restrictions.emplace_back(symbol_index, FUNC(gt), string("ACOIN."));
            authorize_uia_creation.restrictions.emplace_back(symbol_index, FUNC(le), string("ACOIN.ZZZZZZZZZZZZZZZZ"));
            //[
            //  {
            //    "member_index": 2,
            //    "restriction_type": 4,
            //    "argument": [
            //      3,
            //      "ACOIN."
            //    ],
            //    "extensions": []
            //  },
            //  {
            //    "member_index": 2,
            //    "restriction_type": 3,
            //    "argument": [
            //      3,
            //      "ACOIN.ZZZZZZZZZZZZZZZZ"
            //    ],
            //    "extensions": []
            //  }
            //]

            trx.clear();
            trx.operations = {authorize_uia_creation};
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // Bob attempts to create a UIA with a symbol name below the authorized textual range
         // This should fail because it violates Restriction 1
         //////
         {
            asset_create_operation create_uia_op = create_uia("ABCOIN", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, bob_private_key);

            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // Bob attempts to create a UIA with a symobl name above the authorized textual range
         // This should fail because it violates Restriction 2
         //////
         {
            asset_create_operation create_uia_op = create_uia("BOB", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, bob_private_key);

            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // [0,1]: 0 is the rejection_indicator for an index to a sub-restriction; 1 is the index value for Restriction 2
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // Bob attempts to create a sub-token of ACOIN
         // This should succeed because this satisfies the sub-token restriction by Nathan
         //////
         {
            asset_create_operation create_uia_op = create_uia("ACOIN.BOB", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);
            //PUSH_TX(db, trx);
            BOOST_CHECK_THROW( PUSH_TX(db, trx), fc::exception );

            create_uia_op = create_uia("ACOIN.CHARLIE", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);

            create_uia_op = create_uia("ACOIN.DIANA", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // Bob creates his own UIA that is similar to ACOIN
         //////
         {
            upgrade_to_lifetime_member(bob);

            asset_create_operation create_uia_op = create_uia("AACOIN", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);

            PUSH_TX(db, trx);


            create_uia_op = create_uia("AACOIN.TEST", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, nathan_private_key);

            PUSH_TX(db, trx);
         }


         //////
         // Bob attempts to create a sub-token of AACOIN but with Nathan as the issuer
         // This should fail because it violates Restriction 1
         //////
         {
            asset_create_operation create_uia_op = create_uia("AACOIN.BOB", nathan, white_list);
            trx.clear();
            trx.operations = {create_uia_op};
            sign(trx, bob_private_key);

            BOOST_CHECK_THROW(PUSH_TX(db, trx), fc::exception);
            // The failure should indicate the rejection path
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for Restriction 1
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            //EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }

      } FC_LOG_AND_RETHROW()
   }

   /**
    * Test of CAA for account_update_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing a key
    * to ONLY update the voting slate of an account
    */
   BOOST_AUTO_TEST_CASE(authorized_voting_key) {
      try {
         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         ACTORS((nathan));
         fund(nathan, asset(500000 * GRAPHENE_BLOCKCHAIN_PRECISION));
         upgrade_to_lifetime_member(nathan);

         // Arbitrarily identify one of the active witnesses
         flat_set<witness_id_type> witnesses = db.get_global_properties().active_witnesses;
         auto itr_witnesses = witnesses.begin();
         witness_id_type witness0_id = itr_witnesses[0];
         const auto& idx = db.get_index_type<witness_index>().indices().get<by_id>();
         witness_object witness0_obj = *idx.find(witness0_id);


         //////
         // Define a key that can be authorized
         // This can be a new key or an existing key. The existing key may even be the active key of an account.
         //////
         fc::ecc::private_key some_private_key = generate_private_key("some key");
         public_key_type some_public_key = public_key_type(some_private_key.get_public_key());


         //////
         // The key attempts to update the voting slate of Nathan
         // This should fail because the key is not authorized by Nathan to update any part of her account
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();
            account_options nathan_options = nathan.options;
            auto insert_result = nathan_options.votes.insert(witness0_obj.vote_id);
            if (!insert_result.second)
               FC_THROW("Account ${account} was already voting for witness ${witness}",
                        ("account", nathan)("witness", "init0"));
            uop.new_options = nathan_options;

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for Bob's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] { PUSH_TX(db, trx); });
         }


         //////
         // Nathan authorizes the key to update her voting slate
         // by authorizing account updates EXCEPT for
         // updating the owner key
         // updating the active key
         // updating the memo key
         // updating the special owner authority
         // updating the special active authority
         //////
         {
            custom_authority_create_operation authorize_account_update;
            authorize_account_update.account = nathan.get_id();
            authorize_account_update.auth.add_authority(some_public_key, 1);
            authorize_account_update.auth.weight_threshold = 1;
            authorize_account_update.enabled = true;
            authorize_account_update.valid_to = db.head_block_time() + 86400;
            authorize_account_update.operation_type = operation::tag<account_update_operation>::value;

            // Shall not update the owner key member
            auto owner_index = member_index<account_update_operation>("owner");
            restriction no_owner = restriction(owner_index, FUNC(eq), void_t());

            // Shall not update the active key member
            auto active_index = member_index<account_update_operation>("active");
            restriction no_active = restriction(active_index, FUNC(eq), void_t());

            // Shall not update the memo key member of the new_options member
            auto new_options_index = member_index<account_update_operation>("new_options");
            auto memo_index = member_index<account_options>("memo_key");
            restriction same_memo = restriction(new_options_index, FUNC(attr),
                                                vector<restriction>{
                                                        restriction(memo_index, FUNC(eq), nathan.options.memo_key)});

            // Shall not update the extensions member
            auto ext_index = member_index<account_update_operation>("extensions");
            restriction no_ext = restriction(ext_index, FUNC(eq), void_t());

            auto owner_special_index = member_index<account_update_operation::ext>("owner_special_authority");
            restriction no_special_owner = restriction(ext_index, FUNC(attr),
                                                       vector<restriction>{
                                                               restriction(owner_special_index, FUNC(eq), void_t())});

            auto active_special_index = member_index<account_update_operation::ext>("active_special_authority");
            restriction no_special_active = restriction(ext_index, FUNC(attr),
                                                        vector<restriction>{
                                                                restriction(active_special_index, FUNC(eq), void_t())});

            // Shall not update the extensions member of the new_options member
            auto new_options_ext_index = member_index<account_options>("extensions");
            restriction no_new_options_ext = restriction(new_options_index, FUNC(attr), vector<restriction>{
                    restriction(new_options_ext_index, FUNC(eq), void_t())});

            // Combine all of the shall not restrictions
            vector<restriction> shall_not_restrictions = {no_owner, no_active, no_special_owner, no_special_active,
                                                          same_memo};
            authorize_account_update.restrictions = shall_not_restrictions;
            //[
            //  {
            //    "member_index": 2,
            //    "restriction_type": 0,
            //    "argument": [
            //      0,
            //      {}
            //    ],
            //    "extensions": []
            //  },
            //  {
            //    "member_index": 3,
            //    "restriction_type": 0,
            //    "argument": [
            //      0,
            //      {}
            //    ],
            //    "extensions": []
            //  },
            //  {
            //    "member_index": 5,
            //    "restriction_type": 10,
            //    "argument": [
            //      39,
            //      [
            //        {
            //          "member_index": 1,
            //          "restriction_type": 0,
            //          "argument": [
            //            0,
            //            {}
            //          ],
            //          "extensions": []
            //        }
            //      ]
            //    ],
            //    "extensions": []
            //  },
            //  {
            //    "member_index": 5,
            //    "restriction_type": 10,
            //    "argument": [
            //      39,
            //      [
            //        {
            //          "member_index": 2,
            //          "restriction_type": 0,
            //          "argument": [
            //            0,
            //            {}
            //          ],
            //          "extensions": []
            //        }
            //      ]
            //    ],
            //    "extensions": []
            //  },
            //  {
            //    "member_index": 4,
            //    "restriction_type": 10,
            //    "argument": [
            //      39,
            //      [
            //        {
            //          "member_index": 0,
            //          "restriction_type": 0,
            //          "argument": [
            //            5,
            //            "RVP7zsqi7QUAjTAdyynd6DVe8uv4K8gCTRHnAoMN9w9CA1xLCTDVv"
            //          ],
            //          "extensions": []
            //        }
            //      ]
            //    ],
            //    "extensions": []
            //  }
            //]

            // Broadcast the transaction
            trx.clear();
            trx.operations = {authorize_account_update};
            sign(trx, nathan_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // The key attempts to update the owner key for nathan
         // This should fail because it is NOT authorized by nathan
         // It violates Restriction 1 (index-0)
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();

            uop.owner = authority(1, some_public_key, 1);

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_owner_auth);
            // The failure should indicate the rejection path
            // {"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // The key attempts to update the active key for nathan
         // This should fail because it is NOT authorized by nathan
         // It violates Restriction 2 (index-1)
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();

            uop.active = authority(1, some_public_key, 1);

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // {"success":false,"rejection_path":[[0,1],[2,"predicate_was_false"]]}
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,1],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // The key attempts to update the special owner key for nathan
         // This should fail because it is NOT authorized by nathan
         // It violates Restriction 3 (index-2)
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();

            uop.extensions.value.owner_special_authority = no_special_authority();

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_owner_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,2],[0,0],[2,"predicate_was_false"]
            // [0,2]: 0 is the rejection_indicator for an index to a sub-restriction; 2 is the index value for Restriction 3
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,2],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // The key attempts to update the special active key for nathan
         // This should fail because it is NOT authorized by nathan
         // It violates Restriction 4 (index-3)
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();

            uop.extensions.value.active_special_authority = no_special_authority();

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,3],[0,0],[2,"predicate_was_false"]
            // [0,3]: 0 is the rejection_indicator for an index to a sub-restriction; 3 is the index value for Restriction 4
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,3],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // The key attempts to update the memo key for nathan
         // This should fail because it is NOT authorized by nathan
         // It violates Restriction 5 (index-4)
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();

            account_options nathan_options = nathan.options;
            nathan_options.memo_key = some_public_key;
            uop.new_options = nathan_options;

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // "rejection_path":[[0,4],[0,0],[2,"predicate_was_false"]
            // [0,4]: 0 is the rejection_indicator for an index to a sub-restriction; 4 is the index value for Restriction 5
            // [0,0]: 0 is the rejection_indicator for an index to a sub-restriction; 0 is the index value for the only argument
            // [2,"predicate_was_false"]: 0 is the rejection_indicator for rejection_reason; "predicate_was_false" is the reason
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,4],[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // The key attempts to update the voting slate for nathan
         // This should succeed because the key is authorized by nathan
         //////
         {
            account_update_operation uop;
            uop.account = nathan.get_id();
            account_options nathan_options = nathan.options;
            auto insert_result = nathan_options.votes.insert(witness0_obj.vote_id);
            if (!insert_result.second)
               FC_THROW("Account ${account} was already voting for witness ${witness}",
                        ("account", nathan)("witness", "init0"));
            uop.new_options = nathan_options;

            trx.clear();
            trx.operations.emplace_back(std::move(uop));
            sign(trx, some_private_key);
            PUSH_TX(db, trx);
         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of CAA for witness_update_operation
    *
    * Scenario: Test of authorization of one account (nathan) authorizing a key
    * to ONLY change the signing key of a witness account
    */
   BOOST_AUTO_TEST_CASE(authorized_change_witness_signing_key) {
      try {

         //////
         // Initialize the blockchain
         //////
         generate_blocks(HARDFORK_BSIP_40_TIME);
         generate_blocks(5);
         db.modify(global_property_id_type()(db), [](global_property_object &gpo) {
            gpo.parameters.extensions.value.custom_authority_options = custom_authority_options_type();
         });
         set_expiration(db, trx);


         //////
         // Initialize: Accounts
         //////
         // Create a new witness account (witness0)
         ACTORS((witness0));
         // Upgrade witness account to LTM
         upgrade_to_lifetime_member(witness0.id);
         generate_block();

         // Create the witnesses
         // Get the witness0 identifier after a block has been generated
         // to be sure of using the most up-to-date identifier for the account
         const account_id_type witness0_identifier = get_account("witness0").id;
         create_witness(witness0_identifier, witness0_private_key);

         generate_block();

         // Find the witness ID for witness0
         const auto& idx = db.get_index_type<witness_index>().indices().get<by_account>();
         witness_object witness0_obj = *idx.find(witness0_identifier);
         BOOST_CHECK(witness0_obj.witness_account == witness0_identifier);


         //////
         // Define a key that can be authorized
         // This can be a new key or an existing key. The existing key may even be the active key of an account.
         //////
         fc::ecc::private_key some_private_key = generate_private_key("some key");
         public_key_type some_public_key = public_key_type(some_private_key.get_public_key());


         //////
         // Define an alternate witness signing key
         //////
         fc::ecc::private_key alternate_signing_private_key = generate_private_key("some signing key");
         public_key_type alternate_signing_public_key = public_key_type(alternate_signing_private_key.get_public_key());
         // The current signing key should be different than the alternate signing public key
         BOOST_CHECK(witness0_obj.signing_key != alternate_signing_public_key);


         //////
         // The key attempts to update the signing key of witness0
         // This should fail because the key is NOT authorized by witness0 to update the signing key
         //////
         {
            witness_update_operation wop;
            wop.witness = witness0_obj.id;
            wop.witness_account = witness0_obj.witness_account;

            wop.new_signing_key = alternate_signing_public_key;

            trx.clear();
            trx.operations.emplace_back(std::move(wop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should not indicate any rejected custom auths because no CAA applies for the key's attempt
            // "rejected_custom_auths":[]
            EXPECT_EXCEPTION_STRING("\"rejected_custom_auths\":[]", [&] { PUSH_TX(db, trx); });
         }


         //////
         // Nathan authorizes the key to only update the witness signing key
         //////
         {
            custom_authority_create_operation authorize_update_signing_key;
            authorize_update_signing_key.account = witness0.get_id();
            authorize_update_signing_key.auth.add_authority(some_public_key, 1);
            authorize_update_signing_key.auth.weight_threshold = 1;
            authorize_update_signing_key.enabled = true;
            authorize_update_signing_key.valid_to = db.head_block_time() + 86400;
            authorize_update_signing_key.operation_type = operation::tag<witness_update_operation>::value;
            auto url_index = member_index<witness_update_operation>("new_url");
            restriction no_url = restriction(url_index, FUNC(eq), void_t());
            authorize_update_signing_key.restrictions = {no_url};
            //[
            //  {
            //    "member_index": 3,
            //    "restriction_type": 0,
            //    "argument": [
            //      0,
            //      {}
            //    ]
            //  }
            //]

            // Broadcast the transaction
            trx.clear();
            trx.operations = {authorize_update_signing_key};
            sign(trx, witness0_private_key);
            PUSH_TX(db, trx);
         }


         //////
         // The key attempts to update the URL of witness0
         // This should fail because the key is NOT authorized by witness0 to update the URL
         //////
         {
            witness_update_operation wop;
            wop.witness = witness0_obj.id;
            wop.witness_account = witness0_obj.witness_account;

            wop.new_url = "NEW_URL";

            trx.clear();
            trx.operations.emplace_back(std::move(wop));
            sign(trx, some_private_key);
            BOOST_CHECK_THROW(PUSH_TX(db, trx), tx_missing_active_auth);
            // The failure should indicate the rejection path
            // {"success":false,"rejection_path":[[0,0],[2,"predicate_was_false"]]}
            EXPECT_EXCEPTION_STRING("\"rejection_path\":[[0,0],[2,\"predicate_was_false\"]]", [&] {PUSH_TX(db, trx);});
         }


         //////
         // The key attempts to update the signing key of witness0
         // This should succeed because the key is authorized by witness0 to update the signing key
         //////
         {
            witness_update_operation wop;
            wop.witness = witness0_obj.id;
            wop.witness_account = witness0_obj.witness_account;

            wop.new_signing_key = alternate_signing_public_key;

            trx.clear();
            trx.operations.emplace_back(std::move(wop));
            sign(trx, some_private_key);
            PUSH_TX(db, trx);

            // Check the current signing key for witness0
            witness_object updated_witness0_obj = *idx.find(witness0_obj.witness_account);
            BOOST_CHECK(updated_witness0_obj.witness_account == witness0_obj.witness_account);
            BOOST_CHECK(updated_witness0_obj.signing_key == alternate_signing_public_key);
         }

      }
      FC_LOG_AND_RETHROW()
   }

BOOST_AUTO_TEST_SUITE_END()
