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

#include <graphene/chain/vesting_balance_object.hpp>

namespace graphene { namespace chain {

   class ico_balance_object : public abstract_object<ico_balance_object>
   {
      public:
         static constexpr uint8_t space_id = protocol_ids;
         static constexpr uint8_t type_id  = ico_balance_object_type;

         string eth_address;
         asset  balance;
         asset_id_type asset_type()const { return balance.asset_id; }
   };

   struct by_eth_address;

   /**
    * @ingroup object_index
    */
   using ico_balance_multi_index_type = multi_index_container<
      ico_balance_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_eth_address>, composite_key< //?
            ico_balance_object,
            member<ico_balance_object, string, &ico_balance_object::eth_address>,
            const_mem_fun<ico_balance_object, asset_id_type, &ico_balance_object::asset_type>
         > >
      >
   >;

   /**
    * @ingroup object_index
    */
   using ico_balance_index = generic_index<ico_balance_object, ico_balance_multi_index_type>;
} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::ico_balance_object)

FC_REFLECT_TYPENAME( graphene::chain::ico_balance_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::ico_balance_object )
