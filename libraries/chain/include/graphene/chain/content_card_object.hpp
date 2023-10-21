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
        class content_card_object;

        /**
         * @brief This class represents an content card on the object graph
         * @ingroup object
         * @ingroup protocol
         *
         * Content card the primary unit to give and store content information.
         */
        class content_card_object : public graphene::db::abstract_object<content_card_object>
        {
        public:
            static const uint8_t space_id = protocol_ids;
            static const uint8_t type_id  = content_card_object_type;

            account_id_type subject_account;
            string   hash;
            string   url;
            uint64_t timestamp;
            string   type;
            string   description;
            string   content_key;
            string   storage_data;
        };

        struct by_subject_account;
        struct by_subject_account_and_hash;
        struct by_hash;

        typedef multi_index_container<
              content_card_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_subject_account>,
                           composite_key< content_card_object,
                                 member< content_card_object, account_id_type, &content_card_object::subject_account>,
                                 member< object, object_id_type, &object::id>
                           >
                     >,
                     ordered_unique< tag<by_subject_account_and_hash>,
                           composite_key< content_card_object,
                                 member< content_card_object, account_id_type, &content_card_object::subject_account>,
                                 member< content_card_object, string, &content_card_object::hash>
                           >
                     >,
                     ordered_unique< tag<by_hash>,
                           composite_key< content_card_object,
                                 member< content_card_object, string, &content_card_object::hash>,
                                 member< object, object_id_type, &object::id>
                           >
                     >
               >
        > content_card_multi_index_type;

        typedef generic_index<content_card_object, content_card_multi_index_type> content_card_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::content_card_object)
FC_REFLECT_TYPENAME( graphene::chain::content_card_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::content_card_object )
