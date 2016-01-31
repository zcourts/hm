#include "lisp.hpp"
#include "vla.hpp"

#include <algorithm>
#include <sstream>

// #include "debug.hpp"

namespace lisp {

  // this will be useful
  static error unimplemented("unimplemented");

  // special forms
  using special_form = value (*) (environment& env, const list& args);
  
  static value eval_define(environment& env, const list& args);
  static value eval_lambda(environment& env, const list& args);
  static value eval_quote(environment& env, const list& args);
  static value eval_cond(environment& env, const list& args);
  static value eval_begin(environment& env, const list& args);
  static value eval_set(environment& env, const list& args);
  static value eval_defmacro(environment& env, const list& args);
  
  // TODO import ?

  static struct {
	symbol define = "def";
	symbol lambda = "lambda";
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

  
  struct application {

	static const list& to_list(const list& start, const list& ) { return start; }

	template<class Iterator>
	static list to_list(Iterator start, Iterator end) {
	  return make_list(start, end);
	}
	
	// lambda application
	template<class Iterator>
	inline value operator()(const lambda& self, environment&, Iterator arg, Iterator end) const {
	
	  environment sub = shared<environment_type>(self->env);

	  // argument names
	  auto name = self->args.begin(), name_end = self->args.end();	  
	  for(; name != name_end && arg != end; ++name, ++arg) {
		sub->insert( {*name, *arg} );
	  }

	  
	  if( (arg == end && name != name_end) || (name == name_end && arg != end && !self->vararg) ) {
		error e("bad argument count");
		std::stringstream ss;
		ss << "expected: " << self->args.size() << (self->vararg ? "+" : "") << std::endl;
		e.details = ss.str();
		throw e;

	  }
	  
	  // varargs
	  if( self->vararg ) {
		(*sub)[ *self->vararg ] = to_list(arg, end); 
	  }
	  
	  return eval(sub, self->body);
	}


	// builtin application
	template<class Iterator>
	inline value operator()(const builtin& self, environment& env, Iterator arg, Iterator end) const {

	  // call builtin 
	  return self(env, arg, end);
	}
	

	template<class T, class Iterator>
	value operator()(const T& , environment& , Iterator, Iterator ) const {
	  std::stringstream ss;
	  ss << "invalid type in application: " << typeid(T).name();
	  throw error(ss.str());
	}

  };


  struct evaluate {

	// lists
	inline value operator()(const list& self, environment& env) const {
	  
	  if( !self ) throw error("empty list in application");
	  
	  const value& head = self->head;

	  if( head.is<symbol>() ) {

		const symbol& s = head.as<symbol>();
		
		// try special forms
		{
		  auto it = special.find( s );
		  if( it != special.end() ) {
			return it->second(env, self->tail);
		  }
		}

		// try macros
		{
		  auto it = macro.find( s );
		  if( it != macro.end() ) {

			const value expand = application()(it->second, env, self->tail, null);
			
			// debug<2>() << "macro:\t" << value(self) << std::endl
			// 		   << "\t>>\t" << exp << std::endl;
			
			// evaluate result
			return eval(env, expand); 
		  }
		}
	  } 

		
	  
	  // regular function application
	  const value func = eval(env, self->head);

	  // TODO make function c-style variadic and simply push values
	  // onto the stack ?

	  // vla for evaluated args
	  const unsigned argc = length(self->tail);

	  macro_vla(value, args, argc);
	  unsigned i = 0;
	  for(value& v : self->tail) {
		args.data[i] = eval(env, v);
		++i;
	  }
	  
	  return apply(env, func, args.begin(), args.end() );
	}

	
	// variables
	inline value operator()(const symbol& self, environment& env) const {

	  return env->find( self, [&] {
		  throw error("unknown variable: " + std::string(self.name()) );
		});

	}
	
	
	// all the rest is returned as is
	template<class T>
	inline value operator()(const T& self, environment& ) const {
	  return self;
	}
	
  };
  

  // eval
  value eval(environment& env, const value& expr) {
	try { 
	  return expr.apply<value>(evaluate(), env);
	} catch( error& e ) {
	  std::stringstream ss;
	  ss << "  ...  " << expr << '\n' << e.details;
	  e.details = ss.str();
	  throw e;
	}
  }

  
  value apply(environment& env, const value& expr, value* arg, value* end) {
	return expr.apply<value>(application(), env, arg, end);
  }
  
  // special forms
  static value eval_define(environment& env, const list& args) {

	const unsigned argc = length(args);
	
	if( argc != 2 ) {
	  throw error("bad define syntax");
	}
	if( !args->head.is<symbol>() ) {
	  throw error("symbol expected for variable name");
	}

	// TODO eval first ?
	value expr = eval(env, args->tail->head);
	(*env)[ args->head.as<symbol>() ] = expr;
	
	return null;
  }

  
  static value eval_lambda(environment& env, const list& args) {
	static const symbol dot = ".";
	
	const unsigned argc = length(args);
	
	if( argc != 2 ) throw error("bad lambda syntax");
	
	if( !args->head.is<list>() && !args->head.is<symbol>() ) {
	  throw error("expected variable list or symbol for lambda arguments");
	}
	
	lambda res = shared<lambda_type>();
	
	if( args->head.is<symbol>() ) {
	  res->vararg = shared<symbol>(args->head.as<symbol>());
	} else {
	  auto& sub = args->head.as<list>();

	  // check args are symbols
	  for(const auto& a : sub) {
		if( !a.is<symbol>() ) throw error("arguments must be symbols");
	  }
	  
	  for(list it = sub; it; it = it->tail ) {
		auto& s = it->head.as<symbol>();
		
		if( s == dot ) {
		  // varargs
		  list next = it; ++next;
		  list next2 = next; ++next2;
		  
		  if( next2 ) {
			throw error("vararg must be the last argument");
		  }
		  res->vararg = shared<symbol>( next->head.as<symbol>() );
		  break;
			
		} else {
		  // regular args
		  res->args.push_back( s );
		}
	  }
	}

	// body
	res->body = args->tail->head;
	
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
	  return shared<std::string>(self);
	}

	value operator()(const sexpr::list& self) const {
	  return (*this)(self.begin(), self.end());
	}

	template<class Iterator>
	value operator()(Iterator first, Iterator last) const {
	  if( first == last ) return list(nullptr);
	  else return shared<cons>(first->template apply<value>(*this),
							   (*this)(first + 1, last).template as<list>());
	}
	
  };


  value convert(const sexpr::expr& expr) {
	return expr.apply<value>(to_value());
  }
  
  
  
  static value eval_quote(environment&, const list& args) {
	const unsigned argc = length(args);
	
	if(argc != 1) {
	  throw error("bad quote syntax");
	}
	
	return args->head;
  }
  
  
  static value eval_cond(environment& env, const list& args) {

	for(const auto& it : args ) {
	  if(!it.is<list>() || length(it.as<list>()) != 2 ) {
		std::stringstream ss;
		ss << "condition should be a pair: " << it;
		throw error(ss.str());
	  }

	  list cond = it.as<list>();
	  
	  // TODO handle else during canonicalize
	  const value res = eval(env, cond->head);
	  
	  // only false evaluates to false
	  const bool fail = (res.is<bool>() && !res.as<bool>());

	  if(!fail) return eval(env, cond->tail->head);
	}
	
	return null;
  }

  
  
  static value eval_begin(environment& env, const list& args) {
	value res = null;

	for(const auto& x: args) {
	  res = eval(env, x);
	}
	
	return res;
  }

  

  static value eval_defmacro(environment& env, const list& args) {
	const unsigned argc = length(args);
	
	if( argc != 3 ) throw error("bad defmacro syntax");

	if( !args->head.is<symbol>() ) throw error("symbol expected for macro name");
	
	macro[ args->head.as<symbol>() ] = eval_lambda( env, args->tail ).as<lambda>();
	
	return null;
  };

  
  
  static value eval_set(environment& env, const list& args) {
	const unsigned argc = length( args );
	
	if( argc != 2 ) throw error("bad set! syntax");

	if( !args->head.is<symbol>() ) throw error("variable name expected");

	const auto& s = args->head.as<symbol>();
	
	auto& var = env->find(s, [s] {
		throw error("unknown variable " + std::string(s.name()));
	  });

	var = eval(env, args->tail->head);
	return null;
  }
  


  struct stream {
	
	template<class T>
	inline void operator()(const T& self, std::ostream& out) const {
	  out << self;
	}
	
	inline void operator()(const string& self, std::ostream& out) const {
	  out << *self;
	}

	inline void operator()(const list& self, std::ostream& out) const {
	   out << '(';
	
	  bool first = true;
	  for(const auto& xi : self) {
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

	void operator()(const object& obj, std::ostream& out ) const {
	  out << "#<" << obj->type << ">";
	}
  	
	void operator()(const environment& env , std::ostream& out ) const {
	  // out << "#<environment>";
	  out << *env;
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

  object_type::object_type(symbol type, std::initializer_list<value_type> list)
	: unordered_map(list), type(type) { }	
}

