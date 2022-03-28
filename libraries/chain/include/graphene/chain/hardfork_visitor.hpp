#pragma once
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

#include <graphene/protocol/operations.hpp>

#include <graphene/chain/hardfork.hpp>

#include <fc/reflect/typelist.hpp>

#include <type_traits>
#include <functional>

namespace graphene { namespace chain {
using namespace protocol;
namespace TL { using namespace fc::typelist; }

/**
 * @brief The hardfork_visitor struct checks whether a given operation type has been hardforked in or not
 *
 * This visitor can be invoked in several different ways, including operation::visit, typelist::runtime::dispatch, or
 * direct invocation by calling the visit() method passing an operation variant, narrow operation type, operation tag,
 * or templating on the narrow operation type
 */
struct hardfork_visitor {
   using result_type = bool;
   using first_unforked_op = custom_authority_create_operation;
   using BSIP_40_ops = TL::list<custom_authority_create_operation, custom_authority_update_operation,
                                custom_authority_delete_operation>;
   using hf2103_ops = TL::list<ticket_create_operation, ticket_update_operation>;
   using RevPop_40_ops = TL::list<personal_data_create_operation, personal_data_remove_operation,
                                  content_card_create_operation, content_card_update_operation,
                                  content_card_remove_operation, permission_create_operation,
                                  permission_remove_operation, content_vote_create_operation,
                                  content_vote_remove_operation, vote_counter_update_operation,
                                  commit_create_operation, reveal_create_operation>;
   using RevPop_11_ops = TL::list<commit_create_v2_operation, reveal_create_v2_operation>;
   using RevPop_12_ops = TL::list<commit_create_v3_operation, reveal_create_v3_operation>;
   using RevPop_15_ops = TL::list<content_card_v2_create_operation, content_card_v2_update_operation,
                                  content_card_v2_remove_operation, personal_data_v2_create_operation,
                                  personal_data_v2_remove_operation>;
   using RevPop_workers_ops = TL::list<worker_create_operation>;

   fc::time_point_sec now;

   hardfork_visitor(fc::time_point_sec now) : now(now) {}

   /// The real visitor implementations. Future operation types get added in here.
   /// @{
   template<typename Op>
   std::enable_if_t<operation::tag<Op>::value < operation::tag<first_unforked_op>::value, bool>
   visit() { return true; }
   template<typename Op>
   std::enable_if_t<TL::contains<BSIP_40_ops, Op>(), bool>
   visit() { return HARDFORK_BSIP_40_PASSED(now); }
   template<typename Op>
   std::enable_if_t<TL::contains<hf2103_ops, Op>(), bool>
   visit() { return true; }
   template<typename Op>
   std::enable_if_t<TL::contains<RevPop_40_ops, Op>(), bool>
   visit() { return HARDFORK_REVPOP_40_PASSED(now); }
   template<typename Op>
   std::enable_if_t<TL::contains<RevPop_11_ops, Op>(), bool>
   visit() { return HARDFORK_REVPOP_11_PASSED(now); }
   template<typename Op>
   std::enable_if_t<TL::contains<RevPop_12_ops, Op>(), bool>
   visit() { return HARDFORK_REVPOP_12_PASSED(now); }
   template<typename Op>
   std::enable_if_t<TL::contains<RevPop_15_ops, Op>(), bool>
   visit() { return HARDFORK_REVPOP_15_PASSED(now); }
   template<typename Op>
   std::enable_if_t<TL::contains<RevPop_workers_ops, Op>(), bool>
   visit() { return HARDFORK_REVPOP_15_PASSED(now); }
   /// @}

   /// typelist::runtime::dispatch adaptor
   template<class W, class Op=typename W::type>
   std::enable_if_t<TL::contains<operation::list, Op>(), bool>
   operator()(W) { return visit<Op>(); }
   /// static_variant::visit adaptor
   template<class Op>
   std::enable_if_t<TL::contains<operation::list, Op>(), bool>
   operator()(const Op&) { return visit<Op>(); }
   /// Tag adaptor
   bool visit(operation::tag_type tag) {
      return TL::runtime::dispatch(operation::list(), (size_t)tag, *this);
   }
   /// operation adaptor
   bool visit(const operation& op) {
      return visit(op.which());
   }
};

} } // namespace graphene::chain
