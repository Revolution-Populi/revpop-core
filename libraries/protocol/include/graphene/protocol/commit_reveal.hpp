/**
 * The Revolution Populi Project
 * Copyright (C) 2020-2022 Revolution Populi Limited
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

#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/authority.hpp>
#include <graphene/protocol/special_authority.hpp>

namespace graphene { namespace protocol {

   /**
    * @brief Create a commit_reveal data object and set hash field
    *
    * This operation is used to create the commit_reveal_object and set hash field.
    */
   struct commit_create_operation : public base_operation
   {
      struct fee_parameters_type {};

      asset           fee; // always zero

      account_id_type account;
      string          hash;
      uint32_t        maintenance_time;
      public_key_type witness_key;

      account_id_type fee_payer()const { return GRAPHENE_TEMP_ACCOUNT; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& )const;

      void get_required_authorities(vector<authority> &a) const
      {
         a.push_back(authority(1, witness_key, 1));
      }
   };

   /**
    * @brief Set value field of commit-reveal object
    *
    * This operation is used to set value field of the commit_reveal_object.
    */
   struct reveal_create_operation : public base_operation
   {
      struct fee_parameters_type {};

      asset           fee; // always zero

      account_id_type account;
      uint64_t        value;
      uint32_t        maintenance_time;
      public_key_type witness_key;

      account_id_type fee_payer()const { return GRAPHENE_TEMP_ACCOUNT; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& )const;

      void get_required_authorities(vector<authority> &a) const
      {
         a.push_back(authority(1, witness_key, 1));
      }
   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::commit_create_operation::fee_parameters_type,  )
FC_REFLECT( graphene::protocol::commit_create_operation,
            (fee)
            (account)(hash)(maintenance_time)(witness_key)
          )
FC_REFLECT( graphene::protocol::reveal_create_operation::fee_parameters_type,  )
FC_REFLECT( graphene::protocol::reveal_create_operation,
            (fee)
            (account)(value)(maintenance_time)(witness_key)
          )


GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::commit_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::reveal_create_operation )
