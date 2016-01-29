#include "lisp.hpp"
#include "vla.hpp"

#include <algorithm>
#include <sstream>

#include "debug.hpp"

// debug
#include <iostream>

namespace lisp {

  // this will be useful
  static error unimplemented("unimplemented");

  // special forms
  typedef vec<value>::iterator list_iterator;
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

	// symbol unquote = "unquote";	
	// symbol quasiquote = "quasiquote";
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

  
  struct application {
	
	// lambda application
	template<class Iterator>
	inline value operator()(const lambda& self, environment, Iterator arg, Iterator end) const {
	  const unsigned argc = end - arg;

	  // TODO expected/got
	  if(self->vararg && argc < self->args.size() ) {
		throw error("bad argument count");
	  }
	  
	  if(!self->vararg && argc != self->args.size() ) {
		throw error("bad argument count");
	  }

	  
	  // augment captured context with args
	  environment sub = self->env->augment(self->args.begin(), self->args.end(),
										   arg, arg + self->args.size());
	  
	  // varargs
	  if( self->vararg ) {
		(*sub)[ *self->vararg ] = std::make_shared<vec<value>>(arg + self->args.size(), end);
	  }
	  
	  return eval(sub, self->body);
	}


	// builtin application
	template<class Iterator>
	inline value operator()(const builtin& self, environment env, Iterator arg, Iterator end) const {

	  // call builtin 
	  return self(env, arg, end);
	}
	

	template<class T, class Iterator>
	value operator()(const T& , environment , Iterator, Iterator ) const {
	  std::stringstream ss;
	  ss << "invalid type in application: " << typeid(T).name();
	  throw error(ss.str());
	}

  };


  template<class Iterator>
  static value expand(lambda self, Iterator arg, Iterator end) {
	const environment sub = self->env->augment(self->args.begin(), self->args.end(), arg, end);
	return eval(sub, self->body);
  }
  
  

  struct evaluate {

	// lists
	inline value operator()(const list& self, environment env) const {
	  
	  if( self->empty() ) {
		throw error("empty list in application");
	  }

	  const value& head = self->front();
	  
	  if( head.is<symbol>() ) {

		const symbol& s = head.as<symbol>();
		
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

			// expand macro

			value& arg = *(self->begin() + 1);
			const value exp = apply(env, it->second, &arg, &arg + self->size() - 1);
			// const value exp = expand(it->second, );

			debug<2>() << "macro:\t" << value(self) << std::endl
					   << "\t>>\t" << exp << std::endl;
			
			// evaluate result
			return eval(env, exp); 
		  }
		}
	  }
	  
	  // regular function application
	  const value func = eval(env, head);

	  // TODO make function c-style variadic and simply push values
	  // onto the stack ?

	  // vla for evaluated args
	  const unsigned argc = self->size() - 1;
	  macro_vla(value, args, argc);
	  
	  unsigned i = 0;
	  for(auto it = self->begin() + 1, end = self->end(); it != end; ++it, ++i) {
		args.data[i] = eval(env, *it);
	  }
	  
	  return apply(env, func, args.begin(), args.end() );
	}

	
	// variables
	inline value operator()(const symbol& self, environment env) const {

	  return env->find( self, [&] {
		  throw error("unknown variable: " + std::string(self.name()) );
		});

	}
	
	
	// all the rest is returned as is
	template<class T>
	inline value operator()(const T& self, environment ) const {
	  return self;
	}
	
  };
  

  // eval
  value eval(environment env, const value& expr) {
	try { 
	  return expr.apply<value>(evaluate(), env);
	} catch( error& e ) {
	  std::cerr << "when evaluating: " << expr << '\n';
	  throw e;
	}
  }

  
  value apply(environment env, const value& expr, value* arg, value* end) {
	return expr.apply<value>(application(), env, arg, end);
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
	value expr = eval(env, *(arg + 1));
	(*env)[ arg->as<symbol>() ] = expr;
	
	return nil{};
  }

  
  static value eval_lambda(environment env, list_iterator arg, list_iterator end) {
	static const symbol dot = ".";
	
	const unsigned argc = end - arg;
	
	if( argc != 2 ) throw error("bad lambda syntax");
	
	if( !arg->is<list>() && !arg->is<symbol>() ) {
	  throw error("expected variable list or symbol for lambda arguments");
	}
	
	lambda res = shared<lambda_type>();
	
	if( arg->is<symbol>() ) {
	  res->vararg = shared(arg->as<symbol>());
	} else {
	  auto& args = arg->as<list>();

	  // check args are symbols
	  for(const auto& a : *args) {
		if( !a.is<symbol>() ) throw error("arguments must be symbols");
	  }
	  
	  for(auto it = args->begin(), end = args->end(); it != end; ++it ) {
		auto& s = it->as<symbol>();

		if( s == dot ) {
		  // varargs
		  if( it + 2 != end ) {
			throw error("vararg must be the last argument");
		  }
		  res->vararg = shared( (it + 1)->as<symbol>() );
		  break;
			
		} else {
		  // regular args
		  res->args.push_back( s );
		}
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
  
  
  
  static value eval_quote(environment, list_iterator arg, list_iterator end) {
	const unsigned argc = end - arg;

	if(argc != 1) {
	  throw error("bad quote syntax");
	}
	
	return *arg;
  }


  static value eval_cond(environment env, list_iterator arg, list_iterator end) {
	for(auto it = arg; it != end; ++it) {
	  
	  if(!it->is<list>() || it->as<list>()->size() != 2) {
		std::stringstream ss;
		ss << "condition should be a pair: " << *it;
		throw error(ss.str());
	  }

	  const vec<value>& cond = *it->as<list>();
	  
	  // TODO handle else during canonicalize
	  const value res = eval(env, cond[0]);
	  
	  // only false evaluates to false
	  const bool fail = (res.is<bool>() && !res.as<bool>());

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
  
	void operator()(const environment& env, std::ostream& out ) const {
	  out << "#<environment>";
	  // out << *env;
	}

  };

  
  std::ostream& operator<<(std::ostream& out, const environment_type& env) {

	std::function< int( const environment_type& )> fun = [&](const environment_type& env) -> int{

	  int depth = 0;
	  
	  if( env.parent ) {
		depth = 1 + fun(*env.parent);
	  }

	  for(const auto& it : env) {
		for(int i = 0; i < depth; ++i) {
		  out << "  ";
		}
		out << it.first << ": " <<  it.second << '\n';
	  }
	  
	  return depth;
	};
	
	fun(env);
	return out;
  }
  
  
  std::ostream& operator<<(std::ostream& out, const value& x) {
	x.apply( stream(), out);
	return out;
  }


  // this keeps gcc linker happy (??)
  lambda_type::lambda_type() { }


}

