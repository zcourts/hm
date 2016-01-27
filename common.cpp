#include "common.hpp"

#include <set>
#include <ostream>

static std::set<std::string> table;

// symbol symbolize(const std::string& s) {
//   return;
// }

symbol::symbol(const std::string& s) : string(  table.insert(s).first->c_str() ) { }

std::ostream& operator << (std::ostream& out, const symbol& self) {
  return out << self.name();
}
