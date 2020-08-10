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

#include <graphene/protocol/personal_data.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

share_type personal_data_create_operation::calculate_fee( const fee_parameters_type& k )const
{
   return 0;
}


void personal_data_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

share_type personal_data_remove_operation::calculate_fee( const fee_parameters_type& k )const
{
   return 0;
}


void personal_data_remove_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::personal_data_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::personal_data_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::personal_data_remove_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::personal_data_remove_operation )
