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

#include <graphene/chain/content_card_evaluator.hpp>
#include <graphene/chain/vote_master_summary_object.hpp>
#include <graphene/chain/permission_object.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void_result content_card_create_evaluator::do_evaluate( const content_card_create_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.url.empty(), "URL can not be empty.");
   FC_ASSERT(!op.hash.empty(), "Hash can not be empty.");

   const auto& content_idx = d.get_index_type<content_card_index>();
   const auto& content_op_idx = content_idx.indices().get<by_subject_account_and_hash>();

   auto itr = content_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.hash));
   FC_ASSERT(itr->subject_account != op.subject_account || itr->hash != op.hash, "Content card already exists.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type content_card_create_evaluator::do_apply( const content_card_create_operation& o )
{ try {
   database& d = db();
   const auto& new_content_object = d.create<content_card_object>( [&o]( content_card_object& obj )
   {
         obj.subject_account = o.subject_account;
         obj.hash            = o.hash;
         obj.url             = o.url;
         obj.type            = o.type;
         obj.description     = o.description;
         obj.content_key     = o.content_key;
         obj.timestamp       = time_point::now().sec_since_epoch();
         obj.vote_counter    = 0;
   });
   return new_content_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result content_card_update_evaluator::do_evaluate( const content_card_update_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.url.empty(), "URL can not be empty.");
   FC_ASSERT(!op.hash.empty(), "Hash can not be empty.");

   // check personal data already exist
   const auto& content_idx = d.get_index_type<content_card_index>();
   const auto& content_op_idx = content_idx.indices().get<by_subject_account_and_hash>();

   auto itr = content_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.hash));
   FC_ASSERT(itr->subject_account == op.subject_account && itr->hash == op.hash, "Content card does not exists.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type content_card_update_evaluator::do_apply( const content_card_update_operation& o )
{ try {
   database& d = db();

   const auto& content_idx = d.get_index_type<content_card_index>();
   const auto& content_op_idx = content_idx.indices().get<by_subject_account_and_hash>();

   auto itr = content_op_idx.lower_bound(boost::make_tuple(o.subject_account, o.hash));

   d.modify( *itr, [&o](content_card_object& obj){
         obj.subject_account = o.subject_account;
         obj.hash            = o.hash;
         obj.url             = o.url;
         obj.type            = o.type;
         obj.description     = o.description;
         obj.content_key     = o.content_key;
         obj.timestamp       = time_point::now().sec_since_epoch();
   });

   return itr->id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result content_card_remove_evaluator::do_evaluate( const content_card_remove_operation& op )
{ try {
   database& d = db();

   const auto& content_idx = d.get_index_type<content_card_index>();
   const auto& content_op_idx = content_idx.indices().get<by_id>();

   auto itr = content_op_idx.lower_bound(op.content_id);
   FC_ASSERT(itr->id == op.content_id, "Content card does not exists.");
   FC_ASSERT(itr->subject_account == op.subject_account, "Subject account don't have right to remove this content card.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type content_card_remove_evaluator::do_apply( const content_card_remove_operation& o )
{ try {
   database& d = db();

   // remove permissions for content
   const auto content_id = optional<object_id_type>(o.content_id);
   const auto& perm_idx = d.get_index_type<permission_index>();
   const auto& perm_op_idx = perm_idx.indices().get<by_object_id>();
   auto itr = perm_op_idx.lower_bound(content_id);
   while (content_id == itr->object_id) {
      d.remove(d.get_object(itr->id));
      ++itr;
   }

   // remove content card object
   d.remove(d.get_object(o.content_id));

   return o.content_id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result vote_counter_update_evaluator::do_evaluate( const vote_counter_update_operation& op )
{ try {
   database& d = db();
   const auto& vms_idx = d.get_index_type<vote_master_summary_index>();
   const auto& by_master_idx = vms_idx.indices().get<by_master_account>();
   auto itr = by_master_idx.lower_bound(op.master_account);
   FC_ASSERT(itr->master_account == op.master_account, "Master account not found.");
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result vote_counter_update_evaluator::do_apply( const vote_counter_update_operation& o )
{ try {
   database& d = db();
   const auto& content_idx = d.get_index_type<content_card_index>();
   const auto& content_op_idx = content_idx.indices().get<by_id>();

   int vote_counter_sum = 0;
   for (const auto& update_data: o.vote_data) {
      auto itr = content_op_idx.lower_bound(update_data.first);
      if(itr->id == update_data.first){
         d.modify( *itr, [&update_data](content_card_object& obj){
            obj.vote_counter += update_data.second;
         });
         vote_counter_sum += update_data.second;
      }
   }

   // Update vote master summary with new vote created
   const auto& vms_idx = d.get_index_type<vote_master_summary_index>();
   const auto& by_master_idx = vms_idx.indices().get<by_master_account>();
   auto itr = by_master_idx.lower_bound(o.master_account);
   if (itr->master_account == o.master_account) {
      d.modify( *itr, [vote_counter_sum](vote_master_summary_object& obj){
         obj.updated_votes += vote_counter_sum;
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
