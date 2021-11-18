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

#include <vector>
#include <string>

#include "storage_adapter.hpp"

namespace graphene { namespace gateway {

class storage_adapter_ipfs : public storage_adapter
{
public:
   storage_adapter_ipfs(const std::string& url);
   virtual ~storage_adapter_ipfs();

   /** Receive content file and move to the storage
    *  @return link to stored file
    *
    */
   std::string store_content(const file_upload& file) override;

   /**
    *
    *
    */
   void remove_content() override;

   /**
    *
    *
    */
   void get_content_list() override;
private:
   std::string ipfs_node_url;
};

}} // namespace graphene::gateway
