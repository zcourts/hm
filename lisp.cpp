#include "lisp.hpp"
#include "vla.hpp"

#include <algorithm>

// debug
#include <iostream>

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
	std::string define = "def";
	std::string lambda = "fn";
	std::string quote = "quote";
	std::string begin = "do";
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


  // first expr in self has been evaluated, passed as the first
  // parameter with correct type
  struct apply {
	
	// lambda application
	value operator()(const lambda& func, environment env, const sexpr::list& self) const {
	  assert( !self.empty() );

	  // TODO varargs
	  if(func->args.size() != self.size() - 1) {
		throw error("bad argument count");
	  }

	  environment sub;

	  {
		// vla for args 
		macro_vla(value, args, self.size() - 1);

		for(unsigned i = 0; i < args.size; ++i) {
		  args.data[i] = eval(env, self[i + 1]);
		}
	  
		// augment environment from args 
		sub = env->augment(func->args.begin(), func->args.end(),
						   args.begin(), args.end() );
	  }
	  
	  return eval(sub, func->body);

	}


	value operator()(const builtin& func, environment env, const sexpr::list& self) const {

	   // vla for args 
	  macro_vla(value, args, self.size() - 1);

	  for(unsigned i = 0; i < args.size; ++i) {
		args.data[i] = eval(env, self[i + 1]);
	  }
	  
	  // call builtin 
	  return func.ptr(env, args.begin(), args.end() );
	}
	

	template<class T>
	value operator()(const T& , environment , const sexpr::list& ) const {
	  throw error("invalid type in application");
	}

  };


  struct evaluate {

	// lists
	inline value operator()(const sexpr::list& self, environment env) const {
	  
	  if( self.empty() ) {
		throw error("empty list in application");
	  }

	  // try special forms
	  if( self[0].is<symbol>() ) {
		auto it = special.find( self[0].as<symbol>() );
		if( it != special.end() ) return it->second(env, self);
	  }

	  // regular function application
	  value func = eval(env, self[0]);
	  return func.apply<value>( apply(), env, self );
	}

	
	// wrap strings
	value operator()(const sexpr::string& self, environment ) const {
	  return shared(self);
	}

	// variables
	inline value operator()(const symbol& self, environment env) const {

	  // TODO check this during canonicalize
	  // {
	  // 	auto it = special.find( self );
	  // 	if( it != special.end() ) throw error("reserved keyword");
	  // }

	  return env->find( self, [self] {
		  throw error("unknown variable: " + std::string(self) );
		});

	}
	
	
	// all the rest is returned as is
	template<class T>
	inline value operator()(const T& self, environment ) const {
	  return self;
	}
	
  };
  

  value eval(environment env, const sexpr::expr& expr) {
	return expr.apply<value>(evaluate(), env);
  }


  static bool check_special(const sexpr::list& list, const std::string& name) {
	return !list.empty() && list[0].is<symbol>() && list[0].as<symbol>() == name;
  }

  
  // special forms
  static value eval_define(environment env, const sexpr::list& list) {
	assert( check_special(list, keyword.define) );

	if( list.size() != 3 ) {
	  throw error("bad define syntax");
	}

	if( !list[1].is<symbol>() ) {
	  throw error("symbol expected for variable name");
	}

	// TODO eval first ?
	(*env)[ list[1].as<symbol>() ] = eval(env, list[2]);
	
	return nil{};
  }

  
  static value eval_lambda(environment env, const sexpr::list& list) {
	assert( check_special(list, keyword.lambda));
	
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



  struct quote {

	template<class T>
	value operator()(const T& self) const {
	  return self;
	}

	// wrapped
	value operator()(const std::string& self) const {
	  return shared(self);
	}

	value operator()(const sexpr::list& self) const {
	  list res = shared<list_type>();
	  res->reserve( self.size() );
	  
	  std::transform(self.begin(), self.end(),
					 std::back_inserter(*res), [&](const sexpr::expr& e) {
					   return e.apply<value>( quote() );
					 });
	  
	  return res;
	}
	
  };

  
  static value eval_quote(environment , const sexpr::list& list) {
	assert( check_special(list, keyword.quote));

	if(list.size() != 2) {
	  throw error("bad quote syntax");
	}

	return list[1].apply<value>(quote());
  }


  static value eval_cond(environment env, const sexpr::list& list) {
	assert( check_special(list, keyword.cond));

	for(unsigned i = 1, n = list.size(); i < n; ++i) {
	  
	  if(!list[i].is<sexpr::list>() || list[i].as<sexpr::list>().size() != 2) {
		throw error("condition should be a pair");
	  }

	  auto& cond = list[i].as<sexpr::list>();

	  // TODO handle else during canonicalize
	  const value res = eval(env, cond[0]);
	  
	  // only false evaluates to false
	  const bool fail = res.is<bool>() && !res.as<bool>();

	  if(!fail) return eval(env, cond[1]);
	}
	
	return nil{};
  }

  
  static value eval_begin(environment env, const sexpr::list& list) {
	throw unimplemented;
  }

  
  static value eval_set(environment env, const sexpr::list& list) {
	throw unimplemented;
  }



  struct stream {
	
	void operator()(const nil& self, std::ostream& out) const {
	  out << "nil";
	}

	template<class T>
	void operator()(const T& self, std::ostream& out) const {
	  out << self;
	}
	
	void operator()(const string& self, std::ostream& out) const {
	  out << *self;
	}

	void operator()(const list& self, std::ostream& out) const {
	   out << '(';
	
	  bool first = true;
	  for(const auto& xi : *self) {
		if(!first) {
		  out << ' ';
		} else {
		  first = false;
		}
		out << xi;
	  }
	  out << ')';
	}

	// TODO context ?
	void operator()(const lambda&, std::ostream& out ) const {
	  out << "#<lambda>";
	}
  
	void operator()(const builtin&, std::ostream& out ) const {
	 out << "#<builtin>";
	}
  
	void operator()(const environment&, std::ostream& out ) const {
	  out << "#<environment>";
	}
  };
	
  std::ostream& operator<<(std::ostream& out, const value& x) {
	x.apply( stream(), out);
	return out;
  }
  
}

