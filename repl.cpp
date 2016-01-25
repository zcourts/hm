#include "repl.hpp"

#include <readline/readline.h>
#include <readline/history.h>


namespace repl {

  static const char* getline(const char* prompt) {
	const char* line = readline(prompt);
	if (line && *line) add_history( line );
	return line;
  }

  
  void loop(handler_type handler, const char* prompt) {
	const char* line;
	
	 while( (line = getline(prompt)) ) {
	   try {
		 handler(line);
	   } catch( exit& e ) {
		 break;
	   }
	 }
  }



}
