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
            static constexpr uint8_t space_id = protocol_ids;
            static constexpr uint8_t type_id  = commit_reveal_object_type;

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
