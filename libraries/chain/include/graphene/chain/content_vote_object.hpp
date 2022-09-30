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
        class content_vote_object;

        /**
         * @brief This class represents an votes on the object graph
         * @ingroup object
         * @ingroup protocol
         *
         * Vote the primary unit to give and store votes from accounts.
         */
        class content_vote_object : public graphene::db::abstract_object<content_vote_object>
        {
        public:
            static constexpr uint8_t space_id = protocol_ids;
            static constexpr uint8_t type_id  = content_vote_object_type;

            account_id_type subject_account;
            string content_id;
        };

        struct by_subject_account;
        struct by_content_id;

        typedef multi_index_container<
               content_vote_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_subject_account>,
                           composite_key< content_vote_object,
                                 member< content_vote_object, account_id_type, &content_vote_object::subject_account>,
                                 member< content_vote_object, string, &content_vote_object::content_id>
                           >
                     >,
                     ordered_unique< tag<by_content_id>,
                           composite_key< content_vote_object,
                                 member< content_vote_object, string, &content_vote_object::content_id>,
                                 member< object, object_id_type, &object::id >
                           >
                     >
               >
        > content_vote_multi_index_type;

        typedef generic_index<content_vote_object, content_vote_multi_index_type> content_vote_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::content_vote_object)
FC_REFLECT_TYPENAME( graphene::chain::content_vote_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::content_vote_object )
