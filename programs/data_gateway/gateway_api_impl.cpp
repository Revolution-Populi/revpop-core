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
#include <string>

#include <boost/algorithm/string/replace.hpp>

#include <fc/rpc/api_connection.hpp>
#include <fc/git_revision.hpp>

#include "gateway.hpp"
#include "gateway_api_impl.hpp"
#include <graphene/utilities/git_revision.hpp>

#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
#endif

// explicit instantiation for later use
namespace fc {
	template class api<graphene::gateway::gateway_api, identity_member_with_optionals>;
}

/****
 * General methods for gateway impl object (ctor, about, etc.)
 */

namespace graphene { namespace gateway { namespace detail {

   gateway_api_impl::gateway_api_impl( gateway_api& s)
      : self(s)
   {
   }

   gateway_api_impl::~gateway_api_impl()
   {
   }

   /***
    * @brief return basic information about this program
    */
   fc::variant_object gateway_api_impl::about() const
   {
      std::string client_version( graphene::utilities::git_revision_description );
      const size_t pos = client_version.find( '/' );
      if( pos != std::string::npos && client_version.size() > pos )
         client_version = client_version.substr( pos + 1 );

      fc::mutable_variant_object result;
      //result["blockchain_name"]        = BLOCKCHAIN_NAME;
      //result["blockchain_description"] = RVP_BLOCKCHAIN_DESCRIPTION;
      result["client_version"]           = client_version;
      result["graphene_revision"]        = graphene::utilities::git_revision_sha;
      result["graphene_revision_age"]    = fc::get_approximate_relative_time_string( fc::time_point_sec(
                                                 graphene::utilities::git_revision_unix_timestamp ) );
      result["fc_revision"]              = fc::git_revision_sha;
      result["fc_revision_age"]          = fc::get_approximate_relative_time_string( fc::time_point_sec(
                                                 fc::git_revision_unix_timestamp ) );
      result["compile_date"]             = "compiled on " __DATE__ " at " __TIME__;
      result["boost_version"]            = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
      result["openssl_version"]          = OPENSSL_VERSION_TEXT;

      std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
      std::string os = "osx";
#elif defined(__linux__)
      std::string os = "linux";
#elif defined(_MSC_VER)
      std::string os = "win32";
#else
      std::string os = "other";
#endif
      result["build"] = os + " " + bitness;

      return result;
   }

   void gateway_api_impl::quit()
   {
      ilog( "Quitting Gateway ..." );

      throw fc::canceled_exception();
   }

   /** Receive content file and give it to storage adapter.
    *
    * .
    */
   void gateway_api_impl::store_content(const std::vector<file_upload> &files)
   {
         for (auto &f : files) {
               ilog("Storing file: ${name}, path: ${path}", ("name", f.name)("path", f.path));
         }
   }

   /** .
    *
    * .
    */
   void gateway_api_impl::remove_content()
   {
   }

   /** .
    *
    * .
    */
   void gateway_api_impl::get_content_list()
   {
   }

}}} // namespace graphene::gateway::detail
