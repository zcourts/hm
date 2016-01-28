#ifndef UNION_FIND_HPP
#define UNION_FIND_HPP

#include <map>
#include <boost/pending/disjoint_sets.hpp>

template<class T>
class union_find {
  using rank_type =  std::map<T, int>;

  struct parent_type : std::map<T, T> {

	// insert key if not found (this way we don't have to manually add
	// types during type inference: they are added automatically on
	// first use)
	T& operator[](const T& key) {
	  auto it = this->insert( std::make_pair(key, key) );

	  return it.first->second;
	}
	
  };
  
  rank_type rank;
  parent_type parent;

  template<class U> using pm = boost::associative_property_map<U>;

  boost::disjoint_sets< pm<rank_type>, pm<parent_type> > dsets;   
  

public:

  union_find()
	: dsets( pm<rank_type>(rank),
			 pm<parent_type>(parent) ) {
	
  }

  // y becomes representative
  void link(const T& x, const T& y) {
	dsets.link(x, y);
  }

  // find representative for type
  T find(const T& x) {
	return dsets.find_set(x);
  }


  friend std::ostream& operator<<(std::ostream& out, union_find& self) {
	std::map<T, std::vector<T> > classes;

	for(const auto& t : self.parent) {
	  classes[ self.find(t.first) ].push_back(t.first);
	}

	for(const auto& c : classes ) {
	  out << c.first << ":";
	  for(const auto& x : c.second) {
		out << " " << x;
	  }

	  out << '\n';
	}
									
	return out;

  }

  
};



#endif
