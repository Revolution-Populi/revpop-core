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

#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/permission_object.hpp>

namespace graphene { namespace chain {

class permission_create_evaluator : public evaluator<permission_create_evaluator>
{
public:
   typedef permission_create_operation operation_type;

   void_result do_evaluate( const permission_create_operation& o );
   object_id_type do_apply( const permission_create_operation& o ) ;
};

class permission_remove_evaluator : public evaluator<permission_remove_evaluator>
{
public:
   typedef permission_remove_operation operation_type;

   void_result do_evaluate( const permission_remove_operation& o );
   object_id_type do_apply( const permission_remove_operation& o ) ;
};

} } // graphene::chain
