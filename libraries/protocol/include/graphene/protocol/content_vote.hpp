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

#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/authority.hpp>
#include <graphene/protocol/buyback.hpp>
#include <graphene/protocol/special_authority.hpp>
#include <graphene/protocol/content_vote.hpp>

namespace graphene { namespace protocol {

   /**
    * @brief Create a content vote object
    *
    * This operation is used to create the content_vote_object.
    */
   struct content_vote_create_operation : public base_operation
   {
      struct fee_parameters_type { share_type fee = 300000; };

      asset           fee;

      account_id_type subject_account;
      string          content_id;
      account_id_type master_account;
      string          master_content_id;

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
    * @brief Remove a content vote object
    *
    * This operation is used to remove the content_vote_object.
    */
   struct content_vote_remove_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset           fee; // always zero

      account_id_type      subject_account;
      content_vote_id_type vote_id;

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

FC_REFLECT( graphene::protocol::content_vote_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::content_vote_create_operation,
            (fee)
            (subject_account)(content_id)(master_account)(master_content_id)
          )
FC_REFLECT( graphene::protocol::content_vote_remove_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::content_vote_remove_operation,
            (fee)
            (subject_account)(vote_id)
          )


GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::content_vote_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::content_vote_remove_operation )
