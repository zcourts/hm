#ifndef HINDLEY_MILNER_HPP
#define HINDLEY_MILNER_HPP

#include <map>

#include "type.hpp"
#include "ast.hpp"

using context = std::map< ast::var, type::poly >;
type::poly hindley_milner(const context& ctx, const ast::expr& e);


#endif
