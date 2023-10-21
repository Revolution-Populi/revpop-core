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
