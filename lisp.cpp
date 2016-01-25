#include "lisp.hpp"

#include <algorithm>

namespace lisp {


  // special forms
  value eval_define(environment env, const sexpr::list& list);
  value eval_lambda(environment env, const sexpr::list& list);
  value eval_quote(environment env, const sexpr::list& list);
  value eval_if(environment env, const sexpr::list& list);
  value eval_begin(environment env, const sexpr::list& list);
  value eval_set(environment env, const sexpr::list& list);          

  // prototype
  using special_form = value (*) (environment env, const sexpr::list& list);
  
  // TODO import ?

  static std::map<std::string, special_form> special = {
	{"define", eval_define},
	{"lambda", eval_lambda},
	{"quote", eval_quote},   	
	{"if", eval_if},
	{"begin", eval_begin},
	{"set!", eval_set}
  };


  // note: first expr in self has been evaluated as the first
  // parameter
  struct apply {

	// lambda application
	value operator()(const lambda& func, environment env, const sexpr::list& self) const {
	  assert( !self.empty() );

	  // TODO varargs
	  assert(func->args.size() == self.size() - 1);
	  
	  // evaluate args
	  vec<value> args;
	  args.reserve( self.size() - 1);
	  
	  std::transform(self.begin() + 1, self.end(),
					 std::back_inserter(args), [env](const sexpr::expr& e) {
					   return eval(env, e);
					 });

	  // augment environment from args 
	  environment sub = env->augment(func->args.begin(), func->args.end(),
									 args.begin(), args.end());
	  
	  return eval(sub, func->body);
	}


	template<class T>
	value operator()(const T& func, environment env, const sexpr::list& self) const {
	  throw error("invalid type in application");
	}

  };


  struct evaluate {
	
	value operator()(const sexpr::list& self, environment env) const {

	  if( self.empty() ) throw error("empty list in application");

	  // try special forms
	  if( self[0].is<symbol>() ) {
		auto it = special.find(self[0].as<symbol>() );

		if( it != special.end() ) return it->second(env, self);
	  }

	  // regular function application
	  value func = eval(env, self[0]);
	  return func.apply<value>( apply(), env, self );
	}

	// 
	value operator()(const sexpr::string& self, environment env) const {
	  return shared(self);
	}
	

	// all the rest is returned as is
	template<class T>
	value operator()(const T& self, environment env) const {
	  return self;
	}
	
  };
  

  value eval(environment env, sexpr::expr expr) {
	return expr.apply<value>(evaluate(), env);
  }
  

  static error unimplemented("unimplemented");
  
  value eval_define(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  value eval_lambda(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  value eval_quote(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  value eval_if(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  value eval_begin(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  value eval_set(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
}

