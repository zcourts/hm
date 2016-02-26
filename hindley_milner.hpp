#ifndef HINDLEY_MILNER_HPP
#define HINDLEY_MILNER_HPP

#include <map>
#include <set>
#include <stdexcept>

#include "type.hpp"
#include "ast.hpp"
#include "union_find.hpp"


struct type_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};


class context {
  using parent_type = const context*;
  parent_type parent;

  using table_type = std::map< ast::var, type::poly >;
  table_type table;

  using vars_type = std::set< type::var >;
  vars_type vars;

  // constant amortized insertions
  table_type::iterator last = table.end();
public:

  context(parent_type parent = nullptr) : parent(parent) { }

  // hierarchical
  const type::poly& find(const ast::var& var) const;

  // non-hierarchical
  context& set(const ast::var& var, const type::poly&);

  // recursive iterator
  struct iterator {
	const context* c;
	table_type::const_iterator i;

	bool operator!=(const iterator& other) const;
	iterator& operator++();
	const table_type::value_type& operator*() const;
  };

  // is a type variable bound in context ?
  bool bound(const type::var& ) const;

  // bool bound(const ast::var& ) const;

  
  iterator begin() const;
  iterator end() const;

  void insert(iterator first, iterator last);

  struct setter {
	context* owner;
	const ast::var& var;

	void operator=(const type::poly& p) {
	  owner->set(var, p);
	}
  };

  setter operator[](const ast::var& var) {
	return {this, var};
  }
  
};


// generalize monotype t given context ctx i.e. quantify all variables
// unbound in ctx
type::poly generalize(const context& ctx, const type::mono& t, const std::set<type::var>& exclude = {});

// hindley-milner type inference for expression e in context ctx.
type::poly hindley_milner(union_find<type::mono>& types, const context& ctx, const ast::expr& e);


#endif
