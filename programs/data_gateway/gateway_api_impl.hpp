/**
 * The Revolution Populi Project
 * Copyright (C) 2021 Revolution Populi Limited
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

#include <fc/variant_object.hpp>

#include <graphene/app/api.hpp>

#include <graphene/wallet/api_documentation.hpp>

namespace graphene { namespace gateway { 

class gateway_api;

namespace detail {

class gateway_api_impl
{
public:
   graphene::wallet::api_documentation method_documentation;
   gateway_api& self;

   gateway_api_impl( gateway_api& s);

   virtual ~gateway_api_impl();

   /***
    * @brief return basic information about this program
    */
   fc::variant_object about() const;

   void quit();

   /** Receive content file and give it to storage adapter.
    *
    * .
    */
   void store_content(const std::vector<file_upload>& files);

   /** .
    *
    * .
    */
   void remove_content();

   /** .
    *
    * .
    */
   void get_content_list();
};

}}} // namespace graphene::wallet::detail
