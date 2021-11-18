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

#include <graphene/app/api.hpp>

#include "gateway.hpp"
#include "gateway_api_impl.hpp"

namespace graphene { namespace gateway {

void from_variant( const fc::variant& var,  file_upload& vo, uint32_t max_depth )
{
   if (var.is_object()) {
      fc::variant_object var_obj = var.get_object();
      if (var_obj.contains("name") && var_obj["name"].is_string())
         vo.name = var_obj["name"].get_string();
      if (var_obj.contains("path") && var_obj["path"].is_string())
         vo.path = var_obj["path"].get_string();
   }
}

gateway_api::gateway_api(std::unique_ptr<storage_adapter>&& storage)
   : my( std::make_unique<detail::gateway_api_impl>(*this, std::move(storage)) )
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
void gateway_api::store_content(const std::vector<file_upload>& files)
{
   my->store_content(files);
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