/**
 * The Revolution Populi Project
 * Copyright (C) 2021 Revolution Populi Limited
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

#include <graphene/chain/commit_reveal_v3_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/special_authority_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void_result commit_create_v3_evaluator::do_evaluate( const commit_create_v3_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.hash.empty(), "Hash can not be empty.");

   const auto& gpo = d.get_global_properties();
   const auto& dgpo = d.get_dynamic_global_properties();

   FC_ASSERT(op.maintenance_time == dgpo.next_maintenance_time.sec_since_epoch(), "Incorrect maintenance time.");

   const auto& cr_idx = d.get_index_type<commit_reveal_v2_index>();
   const auto& by_cr_acc = cr_idx.indices().get<by_account>();
   auto cr_itr = by_cr_acc.lower_bound(op.account);
   if (cr_itr != by_cr_acc.end() && cr_itr->account == op.account) {
      FC_ASSERT(cr_itr -> maintenance_time != dgpo.next_maintenance_time.sec_since_epoch(), "The commit operation for the current maintenance period has already been received.");
   }

   FC_ASSERT(d.head_block_time() < dgpo.next_maintenance_time - gpo.parameters.maintenance_interval / 2,
         "Commit interval has finished.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type commit_create_v3_evaluator::do_apply( const commit_create_v3_operation& o )
{ try {
   database& d = db();
   const auto& cr_idx = d.get_index_type<commit_reveal_v2_index>();
   const auto& by_cr_acc = cr_idx.indices().get<by_account>();
   auto cr_itr = by_cr_acc.lower_bound(o.account);

   if (cr_itr != by_cr_acc.end() && cr_itr->account == o.account) {
      d.modify(*cr_itr, [&o](commit_reveal_v2_object& obj) {
         obj.hash = o.hash;
         obj.value = 0;
         obj.maintenance_time = o.maintenance_time;
      });
      return cr_itr->id;
   }
   const auto &new_cr_object = d.create<commit_reveal_v2_object>([&o](commit_reveal_v2_object &obj) {
      obj.account = o.account;
      obj.hash = o.hash;
      obj.value = 0;
      obj.maintenance_time = o.maintenance_time;
   });
   return new_cr_object.id;

} FC_CAPTURE_AND_RETHROW((o)) }

void_result reveal_create_v3_evaluator::do_evaluate( const reveal_create_v3_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(op.value != 0, "Value can not be empty.");

   const auto& gpo = d.get_global_properties();
   const auto& dgpo = d.get_dynamic_global_properties();

   const auto head_block_time = d.head_block_time();
   FC_ASSERT(head_block_time > dgpo.next_maintenance_time - gpo.parameters.maintenance_interval / 2 &&
                   head_block_time < dgpo.next_maintenance_time, "Reveal interval has finished.");

   const auto& cr_idx = d.get_index_type<commit_reveal_v2_index>();
   const auto& by_cr_acc = cr_idx.indices().get<by_account>();
   auto cr_itr = by_cr_acc.lower_bound(op.account);

   FC_ASSERT(cr_itr != by_cr_acc.end(), "Commit-reveal object doesn't exist.");
   FC_ASSERT(cr_itr->account == op.account, "Commit-reveal object doesn't exist.");
   FC_ASSERT(cr_itr->value == 0, "The reveal operation for the current maintenance period has already been received.");
   string hash = fc::sha512::hash( std::to_string(op.value) );
   FC_ASSERT(cr_itr->hash == hash, "Hash is broken.");

   FC_ASSERT(op.maintenance_time == dgpo.next_maintenance_time.sec_since_epoch(), "Incorrect maintenance time.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type reveal_create_v3_evaluator::do_apply( const reveal_create_v3_operation& o )
{ try {
   database& d = db();
   const auto& cr_idx = d.get_index_type<commit_reveal_v2_index>();
   const auto& by_cr_acc = cr_idx.indices().get<by_account>();
   auto cr_itr = by_cr_acc.lower_bound(o.account);

   if (cr_itr != by_cr_acc.end() && cr_itr->account == o.account) {
      d.modify(*cr_itr, [&o](commit_reveal_v2_object& obj) {
         obj.value = o.value;
      });
   }
   return cr_itr->id;
} FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
