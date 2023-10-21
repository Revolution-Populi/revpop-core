/*
 * Copyright (c) 2019 Contributors.
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

#include <graphene/protocol/operations.hpp>

#include <fc/reflect/typelist.hpp>

namespace graphene { namespace protocol {
namespace typelist = fc::typelist;

// To make the build gentler on RAM, break the operation list into several pieces to build over several files
using operation_list_1 = static_variant<typelist::builder<>
                                                ::add<transfer_operation>            // 0
                                                ::finalize>;
using operation_list_2 = static_variant<typelist::builder<>
                                                ::add<account_create_operation>      // 1
                                                ::add<account_update_operation>      // 2
                                                ::finalize>;
using operation_list_3 = static_variant<typelist::builder<>
                                                ::add<asset_create_operation>        // 6
                                                ::finalize>;
using operation_list_4 = static_variant<typelist::builder<>
                                                ::add<asset_update_feed_producers_operation> // 9
                                                ::add<asset_issue_operation>                 // 10
                                                ::add<asset_reserve_operation>               // 11
                                                ::finalize>;
using operation_list_5 = static_variant<typelist::builder<>
                                                ::add<asset_publish_feed_operation>          // 15
                                                ::add<witness_update_operation>              // 17
                                                ::finalize>;
using operation_list_6 = static_variant<typelist::builder<>
                                                ::add<vesting_balance_create_operation>      // 28
                                                ::add<vesting_balance_withdraw_operation>    // 29
                                                ::finalize>;
using operation_list_7 = static_variant<typelist::builder<>
                                                ::add<override_transfer_operation>   // 33
                                                ::finalize>;
using operation_list_8 = static_variant<typelist::builder<>
                                                ::add<htlc_create_operation>         // 49
                                                ::add<htlc_redeem_operation>         // 50
                                                ::add<htlc_extend_operation>         // 52
                                                ::finalize>;
using operation_list_9 = static_variant<typelist::builder<>
                                                ::add<limit_order_create_operation>  // 54
                                                ::add<limit_order_cancel_operation>  // 55
                                                ::add<call_order_update_operation>   // 56
                                                ::finalize>;
using unsupported_operations_list = static_variant<typelist::builder<>
                                                ::add<account_whitelist_operation>     // 3
                                                ::add<account_upgrade_operation>       // 4
                                                ::add<account_transfer_operation>      // 5
                                                ::add<asset_update_operation>          // 7
                                                ::add<asset_update_bitasset_operation> // 8
                                                ::add<asset_fund_fee_pool_operation>   // 12
                                                ::add<asset_settle_operation>          // 13
                                                ::add<asset_global_settle_operation>   // 14
                                                ::add<witness_create_operation>        // 16
                                                ::add<proposal_create_operation>       // 18
                                                ::add<proposal_update_operation>       // 19
                                                ::add<proposal_delete_operation>       // 20
                                                ::add<withdraw_permission_create_operation>     // 21
                                                ::add<withdraw_permission_update_operation>     // 22
                                                ::add<withdraw_permission_claim_operation>      // 23
                                                ::add<withdraw_permission_delete_operation>     // 24
                                                ::add<committee_member_create_operation>        // 25
                                                ::add<committee_member_update_operation>        // 26
                                                ::add<committee_member_update_global_parameters_operation>      //27
                                                ::add<custom_operation>                // 30
                                                ::add<assert_operation>                // 31
                                                ::add<balance_claim_operation>         // 32
                                                ::add<asset_settle_cancel_operation>   // 34  // VIRTUAL
                                                ::add<asset_claim_fees_operation>      // 35
                                                ::add<fba_distribute_operation>        // 36  // VIRTUAL
                                                ::add<asset_claim_pool_operation>      // 37
                                                ::add<asset_update_issuer_operation>   // 38
                                                ::add<personal_data_create_operation>  // 39
                                                ::add<personal_data_remove_operation>  // 40
                                                ::add<content_card_create_operation>   // 41
                                                ::add<content_card_update_operation>   // 42
                                                ::add<content_card_remove_operation>   // 43
                                                ::add<permission_create_operation>     // 44
                                                ::add<permission_remove_operation>     // 45
                                                ::add<commit_create_operation>         // 46
                                                ::add<reveal_create_operation>         // 47
                                                ::add<worker_create_operation>         // 48
                                                ::add<htlc_redeemed_operation>         // 51  // VIRTUAL
                                                ::add<htlc_refund_operation>           // 53  // VIRTUAL
                                                ::add<fill_order_operation>            // 57  // VIRTUAL
                                                // New operations are added here // Unsupported
                                                ::add_list<typelist::slice<operation::list,
                                                      typelist::index_of< operation::list,
                                                                          custom_authority_create_operation >() >>
                                                ::finalize>;

object_restriction_predicate<operation> get_restriction_predicate_list_1(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_2(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_3(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_4(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_5(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_6(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_7(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_8(size_t idx, vector<restriction> rs);
object_restriction_predicate<operation> get_restriction_predicate_list_9(size_t idx, vector<restriction> rs);

} } // namespace graphene::protocol
