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

#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene
{
   namespace chain
   {

      uint64_t database::get_commit_reveal_seed(const vector<account_id_type> &accounts) const
      {
         const auto &cr_idx = get_index_type<commit_reveal_index>();
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

      vector<account_id_type> database::filter_commit_reveal_participant(const vector<account_id_type> &accounts) const
      {
         const auto &cr_idx = get_index_type<commit_reveal_index>();
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
