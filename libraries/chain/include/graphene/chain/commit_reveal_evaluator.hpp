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
#include <graphene/chain/commit_reveal_object.hpp>

namespace graphene { namespace chain {

class commit_create_evaluator : public evaluator<commit_create_evaluator>
{
public:
   typedef commit_create_operation operation_type;

   void_result do_evaluate( const commit_create_operation& o );
   object_id_type do_apply( const commit_create_operation& o ) ;
};

class reveal_create_evaluator : public evaluator<reveal_create_evaluator>
{
public:
   typedef reveal_create_operation operation_type;

   void_result do_evaluate( const reveal_create_operation& o );
   object_id_type do_apply( const reveal_create_operation& o ) ;
};

} } // graphene::chain
