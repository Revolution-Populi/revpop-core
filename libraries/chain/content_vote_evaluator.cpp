/**
 * The Revolution Populi Project
 * Copyright (C) 2020 Revolution Populi Limited
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
