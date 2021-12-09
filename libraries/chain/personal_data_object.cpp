/**
 * The Revolution Populi Project
 * Copyright (C) 2020 Revolution Populi Limited
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

#include <graphene/chain/personal_data_object.hpp>
#include <graphene/chain/database.hpp>

#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>

namespace graphene { namespace chain {

} } // graphene::chain

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::personal_data_object,
                    (graphene::db::object),
                    (subject_account)(operator_account)(url)(hash)(storage_data)
                    )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::personal_data_object )
