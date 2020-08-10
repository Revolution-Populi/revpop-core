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

#include <graphene/chain/permission_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void_result permission_create_evaluator::do_evaluate( const permission_create_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.permission_type.empty(), "Permission type can not be empty.");
   FC_ASSERT(!op.content_key.empty(), "Content key can not be empty.");

   const auto& perm_idx = d.get_index_type<permission_index>();
   const auto& perm_op_idx = perm_idx.indices().get<by_subject_account>();

   auto itr = perm_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.permission_type, op.object_id, op.operator_account));
   FC_ASSERT(itr->subject_account != op.subject_account || itr->permission_type != op.permission_type
   || itr->object_id != op.object_id || itr->operator_account != op.operator_account, "Permission already exists.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type permission_create_evaluator::do_apply( const permission_create_operation& o )
{ try {
   database& d = db();
   const auto& new_perm_object = d.create<permission_object>( [&o]( permission_object& obj )
   {
         obj.subject_account  = o.subject_account;
         obj.operator_account = o.operator_account;
         obj.permission_type  = o.permission_type;
         obj.object_id        = o.object_id;
         obj.content_key      = o.content_key;
         obj.timestamp        = time_point::now().sec_since_epoch();;
   });
   return new_perm_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result permission_remove_evaluator::do_evaluate( const permission_remove_operation& op )
{ try {
   database& d = db();

   const auto& perm_idx = d.get_index_type<permission_index>();
   const auto& perm_op_idx = perm_idx.indices().get<by_id>();

   auto itr = perm_op_idx.lower_bound(op.permission_id);
   FC_ASSERT(itr->id == op.permission_id, "Permission does not exists.");
   FC_ASSERT(itr->subject_account == op.subject_account, "Subject account don't have right to remove this permission.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type permission_remove_evaluator::do_apply( const permission_remove_operation& o )
{ try {
   database& d = db();
   d.remove(d.get_object(o.permission_id));
   return o.permission_id;
} FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
