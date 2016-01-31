#include "common.hpp"

#include <set>
#include <ostream>
#include <functional>

static std::set<std::string> table;

symbol::symbol(const std::string& s) : string(  table.insert(s).first->c_str() ) { }
symbol::symbol(const char* s) : string(  table.insert(s).first->c_str() ) { }

std::ostream& operator << (std::ostream& out, const symbol& self) {
  return out << self.name();
}


namespace std {
  std::size_t hash< symbol >::operator()(const symbol& s) const {
	return std::hash<const char*>{}(s.name());
  }
}
