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

#include <graphene/chain/personal_data_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void_result personal_data_create_evaluator::do_evaluate( const personal_data_create_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.url.empty(), "URL can not be empty.");
   FC_ASSERT(!op.hash.empty(), "Hash can not be empty.");

   // check personal data already exist
   const auto& pd_idx = d.get_index_type<personal_data_index>();
   const auto& by_op_idx = pd_idx.indices().get<by_subject_account>();

   if (op.subject_account == op.operator_account){
      auto itr = by_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.operator_account, op.hash));
      FC_ASSERT(itr->subject_account != op.subject_account || itr->operator_account != op.operator_account || itr->hash != op.hash,
                "Personal data already exists.");
   } else {
      auto itr = by_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.operator_account));
      FC_ASSERT(itr->subject_account != op.subject_account || itr->operator_account != op.operator_account,
                "Personal data already exists.");
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type personal_data_create_evaluator::do_apply( const personal_data_create_operation& o )
{ try {
   database& d = db();
   const auto& new_pd_object = d.create<personal_data_object>( [&o]( personal_data_object& obj )
   {
         obj.subject_account  = o.subject_account;
         obj.operator_account = o.operator_account;
         obj.url              = o.url;
         obj.hash             = o.hash;

   });
   return new_pd_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result personal_data_remove_evaluator::do_evaluate( const personal_data_remove_operation& op )
{ try {
   database& d = db();
   // check personal data exist
   const auto& pd_idx = d.get_index_type<personal_data_index>();
   const auto& by_op_idx = pd_idx.indices().get<by_subject_account>();
   auto itr = by_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.operator_account, op.hash));
   FC_ASSERT( itr->subject_account == op.subject_account && itr->operator_account == op.operator_account && itr->hash == op.hash,
         "Personal data does not exists.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type personal_data_remove_evaluator::do_apply( const personal_data_remove_operation& o )
{ try {
   database& d = db();
   const auto& pd_idx = d.get_index_type<personal_data_index>();
   const auto& by_op_idx = pd_idx.indices().get<by_subject_account>();
   auto itr = by_op_idx.lower_bound(boost::make_tuple(o.subject_account, o.operator_account, o.hash));
   auto pd_id = itr->id;
   d.remove(d.get_object(pd_id));
   return pd_id;
} FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
