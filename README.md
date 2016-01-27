a toy implementation of Hindley-Milner type inference in c++11.

```
$ ./hm
> (def first (fn (x y) x))
first :: 'a -> 'b -> 'a
> (first 42 "hello")
(first 42 hello) :: int
> (first 1)
(first 1) :: 'b -> 'a
> (let id (fn (x) x) (id "world"))
(let id (fn (x) x) (id world)) :: string
> 
```

- uses s-expressions for syntax (yay)
- syntax: `def`, `let`, `fn` and function applications as above + literals
- `unit`, `int`, `real`, `string` base types + function types
- no recursion (yet)
- no evaluation (yet)
- no user types (yet)

the primary goal is to study HM type inference, and possibly some
extensions (subtyping, typeclasses, ...)


## dependencies

code:
- boost (spirit for parsing, disjoint_sets for union-find, and program_options)
- readline

building:
- qmake

## building

```
$ qmake
$ make
```

## usage

```
$ ./repl --help
options:
  -h [ --help ]         produce help message
  --lisp                lisp evaluation
  --hm                  hindley-milner type inference (default)
```