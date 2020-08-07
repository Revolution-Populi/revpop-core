#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/content_vote_object.hpp>

namespace graphene { namespace chain {

class content_vote_create_evaluator : public evaluator<content_vote_create_evaluator>
{
public:
   typedef content_vote_create_operation operation_type;

   void_result do_evaluate( const content_vote_create_operation& o );
   object_id_type do_apply( const content_vote_create_operation& o ) ;
};

class content_vote_remove_evaluator : public evaluator<content_vote_remove_evaluator>
{
public:
   typedef content_vote_remove_operation operation_type;

   void_result do_evaluate( const content_vote_remove_operation& o );
   object_id_type do_apply( const content_vote_remove_operation& o ) ;
};

} } // graphene::chain
