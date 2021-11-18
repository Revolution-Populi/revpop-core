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

#include <string>
#include <fc/variant.hpp>
#include <fc/variant_object.hpp>
#include <graphene/app/api.hpp>

namespace graphene { namespace gateway {

typedef struct file_upload {
    std::string name;
    std::string path;
    explicit file_upload(const std::string& name, const std::string& path)
        : name(name), path(path) {}
    file_upload(){}
} file_upload;

// void to_variant( const file_upload& var,  variant& vo, uint32_t max_depth );
void from_variant( const fc::variant& var,  file_upload& vo, uint32_t max_depth );

namespace detail {
class gateway_api_impl;
}

class storage_adapter;

/**
 * This gateway assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching. This API could be provided locally to be used by a web interface.
 */
class gateway_api
{
   public:
      gateway_api(std::unique_ptr<storage_adapter>&& storage);
      virtual ~gateway_api();

      /** Returns info such as client version, git version of graphene/fc, version of boost, openssl.
       * @returns compile time info and client and dependencies versions
       */
      fc::variant_object                    about() const;
      /** Returns a list of all commands supported by the gateway API.
       *
       * This lists each command, along with its arguments and return types.
       * For more detailed help on a single command, use \c gethelp()
       *
       * @returns a multi-line string suitable for displaying on a terminal
       */
      std::string  help()const;

      /** Returns detailed help on a single API command.
       * @param method the name of the API command you want help with
       * @returns a multi-line string suitable for displaying on a terminal
       */
      std::string  gethelp(const std::string& method)const;

      /** Quit from the gateway.
       *
       * The current gateway will be closed and saved.
       */
      void    quit();

      /** Receive content file and give it to storage adapter.
       *
       *
       */
      void store_content(const std::vector<file_upload>& files);

      /**
       *
       *
       */
      void remove_content();

      /**
       *
       *
       */
      void get_content_list();

  private:
      std::shared_ptr<detail::gateway_api_impl> my;
};

} }

extern template class fc::api<graphene::gateway::gateway_api>;

FC_API( graphene::gateway::gateway_api,
        (help)
        (gethelp)
        (about)
        (quit)
        (store_content)
        (remove_content)
        (get_content_list)
      )
