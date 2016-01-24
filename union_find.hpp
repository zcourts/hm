#ifndef UNION_FIND_HPP
#define UNION_FIND_HPP

#include <map>
#include <boost/pending/disjoint_sets.hpp>


template<class T>
class union_find {
  using rank_type =  std::map<T, int>;
  using parent_type = std::map<T, T>;

  rank_type rank;
  parent_type parent;

  template<class U> using pm = boost::associative_property_map<U>;

  boost::disjoint_sets< pm<rank_type>, pm<parent_type> > dsets;   
  

public:

  union_find() : dsets( pm<rank_type>(rank),
						pm<parent_type>(parent) ) {
	
  }
  
  void add(const T& x) {
	dsets.make_set(x);
  }
  
  // y becomes representative
  void link(const T& x, const T& y) {
	dsets.link(x, y);
  }

  // find representative 
  T find(const T& x) {
	return dsets.find_set(x);
  }

  // TODO operator<< for debug ?
  
};



#endif
