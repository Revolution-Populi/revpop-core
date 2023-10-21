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

#include <graphene/chain/content_card_evaluator.hpp>
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
   FC_ASSERT(!op.storage_data.empty(), "Storage data can not be empty.");

   const auto& content_idx = d.get_index_type<content_card_index>();
   const auto& content_op_idx = content_idx.indices().get<by_subject_account_and_hash>();

   auto itr = content_op_idx.lower_bound(boost::make_tuple(op.subject_account, op.hash));
   FC_ASSERT(itr->subject_account != op.subject_account || itr->hash != op.hash, "Content card already exists.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type content_card_create_evaluator::do_apply( const content_card_create_operation& o )
{ try {
   database& d = db();
   const auto& node_properties = d.get_node_properties();
   bool use_full_content_card = node_properties.active_plugins.find("content_cards") != node_properties.active_plugins.end();

   const auto& new_content_object = d.create<content_card_object>( [&o, &use_full_content_card]( content_card_object& obj )
   {
         obj.subject_account = o.subject_account;
         obj.hash            = o.hash;

         if (use_full_content_card) {
            obj.url             = o.url;
            obj.type            = o.type;
            obj.description     = o.description;
            obj.content_key     = o.content_key;
            obj.timestamp       = time_point::now().sec_since_epoch();
            obj.storage_data    = o.storage_data;
         }
   });
   return new_content_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }

void_result content_card_update_evaluator::do_evaluate( const content_card_update_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.url.empty(), "URL can not be empty.");
   FC_ASSERT(!op.hash.empty(), "Hash can not be empty.");
   FC_ASSERT(!op.storage_data.empty(), "Storage data can not be empty.");

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
         obj.storage_data    = o.storage_data;
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

} } // graphene::chain
