#include "restriction_predicate.hxx"
#include "sliced_lists.hxx"

namespace graphene { namespace protocol {
using result_type = object_restriction_predicate<operation>;

result_type get_restriction_predicate_list_13(size_t idx, vector<restriction> rs) {
   return typelist::runtime::dispatch(operation_list_13::list(), idx, [&rs] (auto t) -> result_type {
      using Op = typename decltype(t)::type;
      return [p=restrictions_to_predicate<Op>(std::move(rs), true)] (const operation& op) {
         FC_ASSERT(op.which() == operation::tag<Op>::value,
                   "Supplied operation is incorrect type for restriction predicate");
         return p(op.get<Op>());
      };
   });
}
} }
