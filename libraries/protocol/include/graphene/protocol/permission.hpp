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
#include <graphene/protocol/buyback.hpp>
#include <graphene/protocol/special_authority.hpp>
#include <graphene/protocol/vote.hpp>

namespace graphene { namespace protocol {

   /**
    * @brief Create a permission object
    *
    * This operation is used to create the permission_object.
    */
   struct permission_create_operation : public base_operation
   {
      struct fee_parameters_type { share_type fee = 300000; };

      asset           fee;

      account_id_type subject_account;
      account_id_type operator_account;
      string permission_type;
      optional<object_id_type> object_id;
      string content_key;

      account_id_type fee_payer()const { return subject_account; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& )const;

      void get_required_active_authorities( flat_set<account_id_type>& a )const
      {
         // owner_account should be required anyway as it is the fee_payer(), but we insert it here just to be sure
         a.insert( subject_account );
      }
   };

   /**
    * @brief Remove a permission object
    *
    * This operation is used to remove the permission_object.
    */
   struct permission_remove_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset           fee; // always zero

      account_id_type    subject_account;
      permission_id_type permission_id;

      account_id_type fee_payer()const { return subject_account; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& )const;

      void get_required_active_authorities( flat_set<account_id_type>& a )const
      {
         // owner_account should be required anyway as it is the fee_payer(), but we insert it here just to be sure
         a.insert( subject_account );
      }
   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::permission_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::permission_create_operation,
            (fee)
            (subject_account)(operator_account)(permission_type)(object_id)(content_key)
          )
FC_REFLECT( graphene::protocol::permission_remove_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::permission_remove_operation,
            (fee)
            (subject_account)(permission_id)
          )


GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::permission_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::permission_remove_operation )
