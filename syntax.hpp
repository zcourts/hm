#ifndef SYNTAX_HPP
#define SYNTAX_HPP

#include <stdexcept>

#include "ast.hpp"
#include "sexpr.hpp"

struct syntax_error : std::runtime_error {
  // syntax_error(const std::string& what);
  using std::runtime_error::runtime_error;
};


// transform toplevel sexpr into ast
ast::node transform(const sexpr::expr& e);

#endif
