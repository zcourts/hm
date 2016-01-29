#include "lisp.hpp"
#include "vla.hpp"

#include <algorithm>

// debug
#include <iostream>

namespace lisp {

  // this will be useful
  static error unimplemented("unimplemented");

  // iterators
  typedef vec<value>::iterator list_iterator;
  
  // special forms
  using special_form = value (*) (environment env, list_iterator arg, list_iterator end);
  
  static value eval_define(environment env, list_iterator arg, list_iterator end);
  static value eval_lambda(environment env, list_iterator arg, list_iterator end);
  static value eval_quote(environment env, list_iterator arg, list_iterator end);
  static value eval_cond(environment env, list_iterator arg, list_iterator end);
  static value eval_begin(environment env, list_iterator arg, list_iterator end);
  static value eval_set(environment env, list_iterator arg, list_iterator end);
  static value eval_defmacro(environment env, list_iterator arg, list_iterator end);
  

  // TODO import ?

  static struct {
	symbol define = "def";
	symbol lambda = "fn";
	symbol quote = "quote";
	symbol begin = "do";
	symbol cond = "cond";
	symbol set = "set!";
	symbol defmacro = "defmacro";
  } keyword;

  
  static std::map<symbol, special_form> special = {
	{keyword.define, eval_define},
	{keyword.lambda, eval_lambda},
	{keyword.quote, eval_quote},   	
	{keyword.cond, eval_cond},
	{keyword.begin, eval_begin},
	{keyword.set, eval_set},
	{keyword.defmacro, eval_defmacro}
  };


  // macro table
  static std::map<symbol, lambda> macro;

  
  // first expr in self has been evaluated, passed as the first
  // parameter with correct type
  struct apply {
	
	// lambda application
	template<class Iterator>
	value operator()(const lambda& self, environment env, Iterator arg, Iterator end) const {
	  const unsigned argc = end - arg;

	  // TODO varargs
	  if(self->args.size() != argc) {
		throw error("bad argument count");
	  }
	  
	  environment sub;

	  {
		// vla for args 
		macro_vla(value, args, argc);

		for(Iterator it = arg; it != end; ++it) {
		  const unsigned i = it - arg;
		  args.data[i] = eval(env, *it);
		}
		
		// augment environment from args 
		sub = env->augment(self->args.begin(), self->args.end(),
						   args.begin(), args.end() );
	  }
	  
	  return eval(sub, self->body);

	}


	// builtin application
	template<class Iterator>
	value operator()(const builtin& self, environment env, Iterator arg, Iterator end) const {

	   // vla for args
	  const unsigned argc = end - arg;
	  macro_vla(value, args, argc);

	  for(Iterator it = arg; it != end; ++it) {
		const unsigned i = it - arg;
		args.data[i] = eval(env, *it);
	  }
	  
	  // call builtin 
	  return self.ptr(env, args.begin(), args.end() );
	}
	

	template<class T, class Iterator>
	value operator()(const T& , environment , Iterator, Iterator ) const {
	  throw error("invalid type in application");
	}

  };


  template<class Iterator>
  static sexpr::expr apply_macro(lambda self, Iterator arg, Iterator end) {

	// TODO we need to quote sexpr args to make them values
	environment sub = self->env->augment(self->args.begin(), self->args.end(),
										 arg, end);

	// TODO we need to 
	//  eval(sub, self->body)

	// return res;

  }
  
  

  struct evaluate {

	// lists
	inline value operator()(const list& self, environment env) const {
	  
	  if( self->empty() ) {
		throw error("empty list in application");
	  }

	  const auto& head = self->front();
	  
	  if( head.is<symbol>() ) {

		const auto& s = head.as<symbol>();
		
		// try special forms
		{
		  auto it = special.find( s );
		  if( it != special.end() ) {
			return it->second(env, self->begin() + 1, self->end());
		  }
		}

		// try macros
		{
		  auto it = macro.find( s );
		  if( it != macro.end() ) {

			// build macro application 
			// sexpr::list expr = self;
			// expr[0] = it->second;

			// evaluate macro
			// sexpr::expr res = eval(env, expr);

			// and evaluate result
			// return eval(env, res);
		  }
		}
	  }

	  

	  // regular function application
	  const value func = eval(env, head);
	  return func.apply<value>( apply(), env, self->begin() + 1, self->end() );
	}

	
	// variables
	inline value operator()(const symbol& self, environment env) const {

	  return env->find( self, [self] {
		  throw error("unknown variable: " + std::string(self.name()) );
		});

	}
	
	
	// all the rest is returned as is
	template<class T>
	inline value operator()(const T& self, environment ) const {
	  return self;
	}
	
  };
  

  value eval(environment env, const value& expr) {
	try { 
	  return expr.apply<value>(evaluate(), env);
	} catch( error& e ) {
	  std::cerr << "when evaluating: " << expr << '\n';
	  throw e;
	}
  }


  // special forms
  static value eval_define(environment env, list_iterator arg, list_iterator end) {

	const unsigned argc = end - arg;
	
	if( argc != 2 ) {
	  throw error("bad define syntax");
	}

	if( !arg->is<symbol>() ) {
	  throw error("symbol expected for variable name");
	}

	// TODO eval first ?
	(*env)[ arg->as<symbol>() ] = eval(env, *(arg + 1));
	
	return nil{};
  }

  
  static value eval_lambda(environment env, list_iterator arg, list_iterator end) {
	const unsigned argc = end - arg;
	
	if( argc != 2 ) {
	  throw error("bad lambda syntax");
	}

	// TODO varargs
	if( !arg->is<list>() ) {
	  throw error("expected variable list for lambda");
	}

	lambda res = shared<lambda_type>();

	// args
	for(const auto& s : *arg->as<list>() ) {
	  if( !s.is<symbol>() ) {
		throw error("variable names must be symbols");
	  } else {
		res->args.push_back( s.as<symbol>() );
	  }
	}

	// body
	res->body = *(arg + 1);

	// environment
	res->env = env;

	return res;
  }



  // convert a sexpr::expr into a value
  struct to_value {

	template<class T>
	value operator()(const T& self) const {
	  return self;
	}

	value operator()(const std::string& self) const {
	  return shared(self);
	}

	value operator()(const sexpr::list& self) const {
	  list res = shared< vec<value> >();
	  res->reserve( self.size() );
	  
	  std::transform(self.begin(), self.end(),
					 std::back_inserter(*res), [&](const sexpr::expr& e) {
					   return e.apply<value>( *this );
					 });
	  
	  return res;
	}
	
  };


  value convert(const sexpr::expr& expr) {
	return expr.apply<value>(to_value());
  }
  
  // struct to_sexpr {

  // 	template<class T>
  // 	value operator()(const T& self) const {
  // 	  throw error("can't convert to s-expr");
  // 	}

  // 	sexpr::expr operator()(const string& self) const {
  // 	  return *self;
  // 	}
	
  // 	sexpr::expr operator()(const list& self) const {
  // 	  sexpr::list res;
  // 	  res.reserve( self->size() );
	  
  // 	  std::transform(self->begin(), self->end(),
  // 					 std::back_inserter(*res), [&](const value& v) {
  // 					   return e.apply<sexpr::expr>( *this );
  // 					 });
  // 	  return res;
  // 	}
	
  // };

  
  static value eval_quote(environment, list_iterator arg, list_iterator end) {
	const unsigned argc = end - arg;
	if(argc != 1) {
	  throw error("bad quote syntax");
	}

	return arg->apply<value>(to_value());
  }


  static value eval_cond(environment env, list_iterator arg, list_iterator end) {
	
	for(auto it = arg; it != end; ++it) {
	  
	  if(!it->is<list>() || it->as<list>()->size() != 2) {
		throw error("condition should be a pair");
	  }

	  const vec<value>& cond = *it->as<list>();
	  
	  // TODO handle else during canonicalize
	  const value res = eval(env, cond[0]);
	  
	  // only false evaluates to false
	  const bool fail = res.is<bool>() && !res.as<bool>();

	  if(!fail) return eval(env, cond[1]);
	}
	
	return nil{};
  }

  
  
  static value eval_begin(environment env, list_iterator arg, list_iterator end) {
	
	value res = nil();
	for(auto it = arg; it != end; ++it) {
	  res = eval(env, *it);
	}
	
	return res;
  }

  

  static value eval_defmacro(environment env, list_iterator arg, list_iterator end) {
	const unsigned argc = end - arg;
	
	if( argc != 3 ) throw error("bad defmacro syntax");

	if( !arg->is<symbol>() ) throw error("symbol expected for macro name");

	macro[ arg->as<symbol>() ] = eval_lambda( env, arg + 1, end ).as<lambda>();
	
	return nil();	
  };

  
  
  static value eval_set(environment env, list_iterator arg, list_iterator end) {
	const unsigned argc = end - arg;

	if( argc != 2 ) throw error("bad set! syntax");

	if( !arg->is<symbol>() ) throw error("variable name expected");

	const auto& s = arg->as<symbol>();
	
	auto& var = env->find(s, [s] {
		throw error("unknown variable " + std::string(s.name()));
	  });

	var = eval(env, *(arg + 1));
	return nil();
  }



  struct stream {
	
	void operator()(const nil& , std::ostream& out) const {
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


  // this keeps gcc linker happy (??)
  lambda_type::lambda_type() { }


 
  
}

