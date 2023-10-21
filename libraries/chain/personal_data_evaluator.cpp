/*
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

#include <graphene/chain/personal_data_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/database.hpp>
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
   FC_ASSERT(!op.storage_data.empty(), "Storage data can not be empty.");

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
         obj.storage_data     = o.storage_data;

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
