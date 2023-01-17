/*
 * Copyright (c) 2018-2023 Revolution Populi Limited, and contributors.
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

#include <graphene/chain/content_vote_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/vote_master_summary_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void_result content_vote_create_evaluator::do_evaluate( const content_vote_create_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.content_id.empty(), "Content id can not be empty.");

   const auto& vote_idx = d.get_index_type<content_vote_index>();
   const auto& by_op_idx = vote_idx.indices().get<by_subject_account>();

   auto itr = by_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.content_id));
   FC_ASSERT(itr->subject_account != op.subject_account || itr->content_id != op.content_id,
                "Content vote already exists.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type content_vote_create_evaluator::do_apply( const content_vote_create_operation& o )
{ try {
   database& d = db();
   const auto& new_vote_object = d.create<content_vote_object>( [&o]( content_vote_object& obj )
   {
      obj.subject_account = o.subject_account;
      obj.content_id      = o.content_id;
   });
   // Update vote master summary with new vote created
   const auto& vms_idx = d.get_index_type<vote_master_summary_index>();
   const auto& by_master_idx = vms_idx.indices().get<by_master_account>();
   auto itr = by_master_idx.lower_bound(o.master_account);
   if (itr->master_account == o.master_account) {
      d.modify( *itr, [&o](vote_master_summary_object& obj){
         obj.total_votes++;
      });
   } else {
      d.create<vote_master_summary_object>( [&o]( vote_master_summary_object& obj )
      {
         obj.master_account = o.master_account;
         obj.total_votes    = 1;
         obj.updated_votes  = 0;
      });
   }
   return new_vote_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result content_vote_remove_evaluator::do_evaluate( const content_vote_remove_operation& op )
{ try {
   database& d = db();
   // check personal data exist
   const auto& vote_idx = d.get_index_type<content_vote_index>();
   const auto& by_op_idx = vote_idx.indices().get<by_id>();

   auto itr = by_op_idx.lower_bound(op.vote_id);
   FC_ASSERT( itr->id == op.vote_id, "Content vote does not exists.");
   FC_ASSERT(itr->subject_account == op.subject_account, "Subject account don't have right to remove this content card.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type content_vote_remove_evaluator::do_apply( const content_vote_remove_operation& o )
{ try {
   database& d = db();
   d.remove(d.get_object(o.vote_id));
   return o.vote_id;
} FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
