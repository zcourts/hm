#ifndef PARSE_HPP
#define PARSE_HPP

#include "sexpr.hpp"
#include <istream>

struct parse_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

sexpr::list parse(std::istream& in);


#endif
