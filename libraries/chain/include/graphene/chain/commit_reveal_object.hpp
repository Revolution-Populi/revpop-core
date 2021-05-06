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

        /**
         * @brief This class represents an commit-reveal data on the object graph
         * @ingroup object
         * @ingroup protocol
         *
         * Commit-reveal data the primary unit to give and store commit-reveal object.
         */
        class commit_reveal_object : public graphene::db::abstract_object<commit_reveal_object>
        {
        public:
            static const uint8_t space_id = protocol_ids;
            static const uint8_t type_id  = commit_reveal_object_type;

            account_id_type account;
            string          hash;
            uint64_t        value;
            uint32_t        maintenance_time;
        };

        struct by_account;

        typedef multi_index_container<
               commit_reveal_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_account>,
                           composite_key< commit_reveal_object,
                                 member< commit_reveal_object, account_id_type, &commit_reveal_object::account>
                           >
                     >
               >
        > commit_reveal_multi_index_type;

        typedef generic_index<commit_reveal_object, commit_reveal_multi_index_type> commit_reveal_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::commit_reveal_object)
FC_REFLECT_TYPENAME( graphene::chain::commit_reveal_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::commit_reveal_object )
