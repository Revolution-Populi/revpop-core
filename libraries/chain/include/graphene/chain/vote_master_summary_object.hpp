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

#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/account.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {
        class database;
        class vote_master_summary_object;

        /**
         * @brief This class represents summary counters of votes of master account
         * @ingroup object
         * @ingroup protocol
         *
         * Vote master summary object helps to verify content votes handling made by the master.
         */
        class vote_master_summary_object : public graphene::db::abstract_object<vote_master_summary_object>
        {
        public:
            static const uint8_t space_id = protocol_ids;
            static const uint8_t type_id  = vote_master_summary_object_type;

            account_id_type master_account;
            uint64_t total_votes;
            uint64_t updated_votes;
        };

        struct by_master_account;

        typedef multi_index_container<
               vote_master_summary_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_master_account>,
                           composite_key< vote_master_summary_object,
                                 member< vote_master_summary_object, account_id_type, &vote_master_summary_object::master_account>
                           >
                     >
               >
        > vote_master_summary_multi_index_type;

        typedef generic_index<vote_master_summary_object, vote_master_summary_multi_index_type> vote_master_summary_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::vote_master_summary_object)
FC_REFLECT_TYPENAME( graphene::chain::vote_master_summary_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::vote_master_summary_object )
