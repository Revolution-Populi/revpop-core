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
        class personal_data_object;
        class vesting_balance_object;

        /**
         * @brief This class represents an pensonal data on the object graph
         * @ingroup object
         * @ingroup protocol
         *
         * Personal data the primary unit to give and store permissions to account's personal data.
         */
        class personal_data_object : public graphene::db::abstract_object<personal_data_object>
        {
        public:
            static const uint8_t space_id = protocol_ids;
            static const uint8_t type_id  = personal_data_object_type;

            account_id_type subject_account;
            account_id_type operator_account;
            string url;
            string hash;
            string storage_data;
        };

        struct by_subject_account;
        struct by_operator_account;

        typedef multi_index_container<
               personal_data_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_subject_account>,
                           composite_key< personal_data_object,
                                 member< personal_data_object, account_id_type, &personal_data_object::subject_account>,
                                 member< personal_data_object, account_id_type, &personal_data_object::operator_account>,
                                 member< personal_data_object, string, &personal_data_object::hash>
                           >
                     >,
                     ordered_unique< tag<by_operator_account>,
                           composite_key< personal_data_object,
                                 member< personal_data_object, account_id_type, &personal_data_object::operator_account>,
                                 member< personal_data_object, account_id_type, &personal_data_object::subject_account>,
                                 member< personal_data_object, string, &personal_data_object::hash>
                           >
                     >
               >
        > personal_data_multi_index_type;

        typedef generic_index<personal_data_object, personal_data_multi_index_type> personal_data_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::personal_data_object)
FC_REFLECT_TYPENAME( graphene::chain::personal_data_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::personal_data_object )
