
- make sure that flagging expansive expressions with io + restricting
  polymorphism for contravariant type variables is sound
  
- implement didier remy's efficient generalization using
  `std::shared_ptr` in type contexts and `std::shared_ptr::use_count`?
  it seems that type variables are free whenever `use_count == 1`,
  otherwise they are referenced in parent environments therefore
  bound. 

- implement actual evaluation
  - LLVM bitcode + JIT ?
  - compilation to C ?
  - dynamic evaluation ?

- macro expansion phase to ease syntax a little bit (defn, dangling
  else in sequences, ...)

