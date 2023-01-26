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
            static constexpr uint8_t space_id = protocol_ids;
            static constexpr uint8_t type_id  = vote_master_summary_object_type;

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
