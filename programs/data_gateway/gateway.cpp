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
// #include <algorithm>
// #include <cctype>
// #include <iomanip>
// #include <iostream>
// #include <iterator>
// #include <sstream>
// #include <string>
// #include <list>

// #include <boost/version.hpp>
// #include <boost/lexical_cast.hpp>
// #include <boost/algorithm/string/replace.hpp>

// #include <boost/range/adaptor/map.hpp>
// #include <boost/range/algorithm_ext/erase.hpp>
// #include <boost/range/algorithm/unique.hpp>
// #include <boost/range/algorithm/sort.hpp>

// #include <boost/multi_index_container.hpp>
// #include <boost/multi_index/ordered_index.hpp>
// #include <boost/multi_index/mem_fun.hpp>
// #include <boost/multi_index/member.hpp>
// #include <boost/multi_index/random_access_index.hpp>
// #include <boost/multi_index/tag.hpp>
// #include <boost/multi_index/sequenced_index.hpp>
// #include <boost/multi_index/hashed_index.hpp>

// #include <fc/git_revision.hpp>
// #include <fc/io/fstream.hpp>
// #include <fc/io/json.hpp>
// #include <fc/io/stdio.hpp>
// #include <fc/network/http/websocket.hpp>
// #include <fc/rpc/cli.hpp>
// #include <fc/rpc/websocket_api.hpp>
// #include <fc/crypto/hex.hpp>
// #include <fc/thread/mutex.hpp>
// #include <fc/thread/scoped_lock.hpp>
// #include <fc/crypto/base58.hpp>
// #include <fc/crypto/aes.hpp>

#include <graphene/app/api.hpp>
// #include <graphene/app/util.hpp>
// #include <graphene/chain/asset_object.hpp>
// #include <graphene/protocol/fee_schedule.hpp>
// #include <graphene/protocol/pts_address.hpp>
// #include <graphene/chain/hardfork.hpp>
// #include <graphene/utilities/git_revision.hpp>
// #include <graphene/utilities/key_conversion.hpp>
// #include <graphene/utilities/words.hpp>
#include "gateway.hpp"
// #include <graphene/wallet/api_documentation.hpp>
#include "gateway_api_impl.hpp"
// #include <graphene/debug_witness/debug_api.hpp>

// #include "operation_printer.hpp"
// #include <graphene/wallet/reflect_util.hpp>

namespace graphene { namespace gateway {

gateway_api::gateway_api()
   : my( std::make_unique<detail::gateway_api_impl>(*this) )
{
}

gateway_api::~gateway_api()
{
}

fc::variant_object gateway_api::about() const
{
    return my->about();
}

void gateway_api::quit()
{
   my->quit();
}

std::string gateway_api::help()const
{
   std::vector<std::string> method_names = my->method_documentation.get_method_names();
   std::stringstream ss;
   for (const std::string& method_name : method_names)
   {
      try
      {
         ss << my->method_documentation.get_brief_description(method_name);
      }
      catch (const fc::key_not_found_exception&)
      {
         ss << method_name << " (no help available)\n";
      }
   }
   return ss.str();
}

std::string gateway_api::gethelp(const std::string& method)const
{
   // fc::api<gateway_api> tmp;
   std::stringstream ss;
   ss << "\n";

   std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
   if (!doxygenHelpString.empty())
      ss << doxygenHelpString << "\n";

   else if (doxygenHelpString.empty())
      ss << "No help defined for method " << method << "\n";

   return ss.str();
}

/** Receive content file and give it to storage adapter.
 *
 * .
 */
void gateway_api::store_content()
{
   my->store_content();
}

/** .
 *
 * .
 */
void gateway_api::remove_content()
{
   my->remove_content();
}

/** .
 *
 * .
 */
void gateway_api::get_content_list()
{
   my->get_content_list();
}

}} // namespace graphene::gateway