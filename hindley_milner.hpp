#ifndef HINDLEY_MILNER_HPP
#define HINDLEY_MILNER_HPP

#include <map>
#include <stdexcept>
#include "type.hpp"
#include "ast.hpp"


struct type_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};


// TODO make contexts hierarchical to avoid copies
using context = std::map< ast::var, type::poly >;

// generalize monotype t given context ctx i.e. quantify all variables
// unbound in ctx
type::poly generalize(const context& ctx, type::mono t);

// hindley-milner type inference for expression e in context ctx.
type::poly hindley_milner(const context& ctx, const ast::expr& e);


#endif
