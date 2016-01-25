#ifndef REPL_HPP
#define REPL_HPP

#include "sexpr.hpp"
#include <functional>

namespace repl {

  struct exit : std::exception { };

  using handler_type = std::function<void(const char* line)>;

  // throw exit to quit
  void loop(handler_type handler, const char* prompt = "> ");
  
}
#endif
