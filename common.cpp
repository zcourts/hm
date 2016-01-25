#include "common.hpp"

#include <set>

static std::set<std::string> table;

symbol symbolize(const std::string& s) {
  return table.insert(s).first->c_str();
}

