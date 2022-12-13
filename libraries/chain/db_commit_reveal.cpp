/**
 * The Revolution Populi Project
 * Copyright (c) 2018-2022 Revolution Populi Limited, and contributors.
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

#include <graphene/chain/database.hpp>

namespace graphene
{
   namespace chain
   {

      uint64_t database::get_commit_reveal_seed(const vector<account_id_type> &accounts) const
      {
         const auto &cr_idx = get_index_type<commit_reveal_index>();
         const auto &by_op_idx = cr_idx.indices().get<by_account>();

         uint64_t seed = 0;
         for (const auto &acc : accounts)
         {
            auto itr = by_op_idx.lower_bound(acc);
            if (itr != by_op_idx.end() && itr->account == acc)
            {
               seed += itr->value;
            }
         }
         return seed;
      }

      vector<account_id_type> database::filter_commit_reveal_participant(const vector<account_id_type> &accounts) const
      {
         const auto &cr_idx = get_index_type<commit_reveal_index>();
         const auto &by_op_idx = cr_idx.indices().get<by_account>();

         vector<account_id_type> result;
         for (const auto &acc : accounts)
         {
            auto itr = by_op_idx.lower_bound(acc);
            if (itr != by_op_idx.end() && itr->account == acc && itr->value != 0)
            {
               result.push_back(itr->account);
            }
         }
         return result;
      }

      uint64_t database::get_commit_reveal_seed_v2(const vector<account_id_type> &accounts) const
      {
         const auto &cr_idx = get_index_type<commit_reveal_v2_index>();
         const auto &by_op_idx = cr_idx.indices().get<by_account>();

         uint64_t seed = 0;
         uint32_t maintenance_time = get_dynamic_global_properties().next_maintenance_time.sec_since_epoch();
         uint32_t prev_maintenance_time = maintenance_time - get_global_properties().parameters.maintenance_interval;
         for (const auto &acc : accounts)
         {
            auto itr = by_op_idx.lower_bound(acc);
            if (itr != by_op_idx.end() && itr->account == acc && prev_maintenance_time <= itr->maintenance_time && itr->maintenance_time < maintenance_time)
            {
               seed += itr->value;
            }
         }
         return seed;
      }

      vector<account_id_type> database::filter_commit_reveal_participant_v2(const vector<account_id_type> &accounts) const
      {
         const auto &cr_idx = get_index_type<commit_reveal_v2_index>();
         const auto &by_op_idx = cr_idx.indices().get<by_account>();

         vector<account_id_type> result;
         uint32_t maintenance_time = get_dynamic_global_properties().next_maintenance_time.sec_since_epoch();
         uint32_t prev_maintenance_time = maintenance_time - get_global_properties().parameters.maintenance_interval;
         for (const auto &acc : accounts)
         {
            auto itr = by_op_idx.lower_bound(acc);
            if (itr != by_op_idx.end() && itr->account == acc && itr->value != 0 && prev_maintenance_time <= itr->maintenance_time && itr->maintenance_time < maintenance_time)

            {
               result.push_back(itr->account);
            }
         }
         return result;
      }

   }
}
