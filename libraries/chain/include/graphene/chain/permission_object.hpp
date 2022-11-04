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
        class permission_object;

        /**
         * @brief This class represents an permissions on the object graph
         * @ingroup object
         * @ingroup protocol
         *
         * Permission the primary unit to give and store permissions to account's content data.
         */
        class permission_object : public graphene::db::abstract_object<permission_object>
        {
        public:
            static constexpr uint8_t space_id = protocol_ids;
            static constexpr uint8_t type_id  = permission_object_type;

            account_id_type subject_account;
            account_id_type operator_account;
            string permission_type;
            optional<object_id_type> object_id;
            uint64_t timestamp;
            string content_key;
        };

        struct by_subject_account;
        struct by_operator_account;
        struct by_object_id;

        typedef multi_index_container<
              permission_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_subject_account>,
                           composite_key< permission_object,
                                 member< permission_object, account_id_type, &permission_object::subject_account>,
                                 member< permission_object, string, &permission_object::permission_type>,
                                 member< permission_object, optional<object_id_type>, &permission_object::object_id>,
                                 member< permission_object, account_id_type, &permission_object::operator_account>
                           >
                     >,
                     ordered_unique< tag<by_operator_account>,
                           composite_key< permission_object,
                                 member< permission_object, account_id_type, &permission_object::operator_account>,
                                 member< object, object_id_type, &object::id>
                           >
                     >,
                     ordered_unique< tag<by_object_id>,
                           composite_key< permission_object,
                                 member< permission_object, optional<object_id_type>, &permission_object::object_id>,
                                 member< object, object_id_type, &object::id>
                           >
                     >
               >
        > permission_multi_index_type;

        typedef generic_index<permission_object, permission_multi_index_type> permission_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::permission_object)
FC_REFLECT_TYPENAME( graphene::chain::permission_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::permission_object )
