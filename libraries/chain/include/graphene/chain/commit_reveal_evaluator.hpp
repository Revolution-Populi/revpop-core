/*
 * Copyright (c) 2020-2023 Revolution Populi Limited, and contributors.
 * 
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
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
