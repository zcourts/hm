#include "lisp.hpp"

#include <algorithm>

namespace lisp {

  // this will be useful
  static error unimplemented("unimplemented");

  
  // special forms
  static value eval_define(environment env, const sexpr::list& list);
  static value eval_lambda(environment env, const sexpr::list& list);
  static value eval_quote(environment env, const sexpr::list& list);
  static value eval_cond(environment env, const sexpr::list& list);
  static value eval_begin(environment env, const sexpr::list& list);
  static value eval_set(environment env, const sexpr::list& list);          

  // prototype
  using special_form = value (*) (environment env, const sexpr::list& list);
  
  // TODO import ?

  static struct {
	std::string define = "define";
	std::string lambda = "lambda";
	std::string quote = "quote";
	std::string begin = "begin";
	std::string cond = "cond";
	std::string set = "set!";
  } keyword;
  
  static std::map<std::string, special_form> special = {
	{keyword.define, eval_define},
	{keyword.lambda, eval_lambda},
	{keyword.quote, eval_quote},   	
	{keyword.cond, eval_cond},
	{keyword.begin, eval_begin},
	{keyword.set, eval_set}
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

	// lists
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

	// wrap strings
	value operator()(const sexpr::string& self, environment env) const {
	  return shared(self);
	}

	// variables
	value operator()(const symbol& self, environment env) const {

	  {
		auto it = special.find( self );
		if( it != special.end() ) throw error("reserved keyword");
	  }

	  auto it = env->find( self );
	  if( it == env->end() ) {
		throw error("unknown variable");
	  }

	  return it->second;
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
  
  // special forms
  static value eval_define(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  static value eval_lambda(environment env, const sexpr::list& list) {
	assert(!list.empty() && list[0].is<symbol>() && list[0].as<symbol>() == keyword.lambda);
	
	if( list.size() != 3 ) {
	  throw error("bad lambda syntax");
	}

	// TODO varargs
	if( !list[1].is<sexpr::list>() ) {
	  throw error("expected variable list for lambda");
	}

	lambda res = shared<lambda_type>();

	// args
	for(const auto& s : list[1].as<sexpr::list>() ) {
	  if( !s.is<symbol>() ) {
		throw error("variable names must be symbols");
	  } else {
		res->args.push_back( s.as<symbol>() );
	  }
	}

	// body
	res->body = list[2];

	// environment
	res->env = env;

	return res;
  }
  
  static value eval_quote(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  static value eval_cond(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  static value eval_begin(environment env, const sexpr::list& list) {
	throw unimplemented;
  }
  
  static value eval_set(environment env, const sexpr::list& list) {
	throw unimplemented;
  }



  // output
  std::ostream& operator<<(std::ostream& out, const nil& ) {
	return out << "'()";
  }
  
  std::ostream& operator<<(std::ostream& out, const string& s) {
	return out << *s;
  }
  
  std::ostream& operator<<(std::ostream& out, const lambda& f) {
	return out << "#<lambda>";
  }

  std::ostream& operator<<(std::ostream& out, const environment& e) {
	return out << "#<environment>";
  }


  
}

