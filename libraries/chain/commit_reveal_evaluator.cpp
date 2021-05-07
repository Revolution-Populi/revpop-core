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

#include <graphene/chain/commit_reveal_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/special_authority_object.hpp>

#include <algorithm>

namespace graphene { namespace chain {

void_result commit_create_evaluator::do_evaluate( const commit_create_operation& op )
{ try {
   database& d = db();
   FC_ASSERT(!op.hash.empty(), "Hash can not be empty.");

   const auto& gpo = d.get_global_properties();
   const auto& dgpo = d.get_dynamic_global_properties();

   FC_ASSERT(d.head_block_time() < dgpo.next_maintenance_time - gpo.parameters.maintenance_interval / 2,
         "Commit interval has finished.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type commit_create_evaluator::do_apply( const commit_create_operation& o )
{ try {
   database& d = db();
   const auto& cr_idx = d.get_index_type<commit_reveal_index>();
   const auto& by_op_idx = cr_idx.indices().get<by_account>();
   auto itr = by_op_idx.lower_bound(o.account);

   if (itr->account == o.account) {
      d.modify(*itr, [&o](commit_reveal_object& obj) {
         obj.hash = o.hash;
         obj.value = 0;
      });
      return itr->id;
   }
   const auto &new_cr_object = d.create<commit_reveal_object>([&o](commit_reveal_object &obj) {
      obj.account = o.account;
      obj.hash = o.hash;
      obj.value = 0;
   });
   return new_cr_object.id;

} FC_CAPTURE_AND_RETHROW((o)) }

void_result reveal_create_evaluator::do_evaluate( const reveal_create_operation& op )
{ try {
   database& d = db();
   const auto& gpo = d.get_global_properties();
   const auto& dgpo = d.get_dynamic_global_properties();

   const auto head_block_time = d.head_block_time();
   FC_ASSERT(head_block_time > dgpo.next_maintenance_time - gpo.parameters.maintenance_interval / 2 &&
                   head_block_time < dgpo.next_maintenance_time, "Reveal interval has finished.");

   const auto& cr_idx = d.get_index_type<commit_reveal_index>();
   const auto& by_op_idx = cr_idx.indices().get<by_account>();
   auto itr = by_op_idx.lower_bound(op.account);

   FC_ASSERT(itr->account == op.account, "Commit-reveal object doesn't exist.");
   string hash = fc::sha512::hash( std::to_string(op.value) );
   FC_ASSERT(itr->hash == hash, "Commit-reveal object doesn't exist.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type reveal_create_evaluator::do_apply( const reveal_create_operation& o )
{ try {
   database& d = db();
   const auto& cr_idx = d.get_index_type<commit_reveal_index>();
   const auto& by_op_idx = cr_idx.indices().get<by_account>();
   auto itr = by_op_idx.lower_bound(o.account);

   if (itr->account == o.account) {
      d.modify(*itr, [&o](commit_reveal_object& obj) {
         obj.value = o.value;
      });
   }
   return itr->id;
} FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
