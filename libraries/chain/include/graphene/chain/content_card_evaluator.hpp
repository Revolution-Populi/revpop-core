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
#include <graphene/chain/content_card_object.hpp>

namespace graphene { namespace chain {

class content_card_create_evaluator : public evaluator<content_card_create_evaluator>
{
public:
   typedef content_card_create_operation operation_type;

   void_result do_evaluate( const content_card_create_operation& o );
   object_id_type do_apply( const content_card_create_operation& o );
};

class content_card_update_evaluator : public evaluator<content_card_update_evaluator>
{
public:
   typedef content_card_update_operation operation_type;

   void_result do_evaluate( const content_card_update_operation& o );
   object_id_type do_apply( const content_card_update_operation& o );
};

class content_card_remove_evaluator : public evaluator<content_card_remove_evaluator>
{
public:
   typedef content_card_remove_operation operation_type;

   void_result do_evaluate( const content_card_remove_operation& o );
   object_id_type do_apply( const content_card_remove_operation& o );
};

class vote_counter_update_evaluator : public evaluator<vote_counter_update_evaluator>
{
public:
   typedef vote_counter_update_operation operation_type;

   void_result do_evaluate( const vote_counter_update_operation& o );
   void_result do_apply( const vote_counter_update_operation& o );
};

} } // graphene::chain
