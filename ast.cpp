#include "ast.hpp"


namespace ast {

  seq::with::with(const var& id, const expr& e)
	: id(id), def( e ? e : id ) {

  }

  // (bind def (lambda (id) e))
  struct bind {

	app operator()(const expr& self, const expr& e) const {
	  return make(self, {}, e);
	}

	app operator()(const seq::with& self, const expr& e) const {
	  return make(self.def, self.id, e);
	}
	
	static app make(const expr& def, var id, const expr& e)  {

	  // TODO make these configurable ?
	  static var bind("bind");
	  static var none("_");	  

	  if(!id.name()) id = none;
	  
	  abs lambda = { {id}, shared<expr>(e) };
	  return app { shared<expr>( bind ), {def, lambda} };
	}
	
  };
  
 
  
  template<class Iterator>
  static expr monad(Iterator first, Iterator last) {
	
	if( first + 1 == last ) {
	  if(!first->template is<expr>()) {
		throw std::logic_error("terminal with");
	  } else {
		return first->template as<expr>();
	  }
	} else {
	  return first->template apply<app>(bind(), monad(first + 1, last));
	}
  }
  

  expr seq::monad() const {
	return ast::monad(terms.begin(), terms.end());
  }
 

}
