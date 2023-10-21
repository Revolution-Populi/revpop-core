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
