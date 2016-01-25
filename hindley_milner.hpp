#ifndef HINDLEY_MILNER_HPP
#define HINDLEY_MILNER_HPP

#include <map>
#include <stdexcept>
#include "type.hpp"
#include "ast.hpp"

using context = std::map< ast::var, type::poly >;


struct type_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

type::poly hindley_milner(const context& ctx, const ast::expr& e);


#endif
