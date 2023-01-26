/*
 * Copyright (c) 2018-2023 Revolution Populi Limited, and contributors.
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
