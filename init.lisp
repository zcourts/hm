
;; some helpers
(def not (lambda (x) (cond (x false) (true true))))


;; list functions
(def list-length (lambda (x)
			  (cond ((null? x) 0)
					('else (number-add 1 (list-length (cdr x)))))))

(def cadr (lambda (x) (car (cdr x))))

(def list-map (lambda (x f)
		   (cond ((null? x) x)
				 ('else (cons (f (car x)) (list-map (cdr x) f))))))

(def list-each (lambda (x f)
				(cond ((null? x) '())
					  ('else (do (f (car x)) (list-each (cdr x) f))))))

(def list (lambda args args))

(def list-append (lambda (lhs rhs)
                         (cond ((null? lhs) rhs)
                               ('else (cons (car lhs) (list-append (cdr lhs) rhs))))))


(def nth (lambda (x n)
		   (cond ((null? x) (error "index out of bounds"))
				 ((number=? n 0) (car x))
				 ('else (nth (cdr x) (number-sub n 1))))))


;; TODO unquote-splicing ?
(defmacro quasiquote (e)
  (cond
    ;; not a list: just quote
    ((not (list? e)) (list 'quote e))
    ;; empty list: as is
    ((null? e) (list 'quote e))

    ;; unquote
    ((eq? (car e) 'unquote)
     (cond ((number=? (list-length e) 2) (cadr e))
           ('else (error "bad unquote syntax"))))
    ('else
     ;; (quasiquote (a b c)) => (list (quasiquote a) (quasiquote b) (quasiquote c))
     (cons 'list (list-map e (lambda (x) (list 'quasiquote x)))))))

(defmacro if (test conseq alt) 
  `(cond (,test ,conseq) (true ,alt)))

(defmacro or (head . tail)
  (if (null? head) head
      `(if ,head true ,(cons 'or tail))))

(defmacro and (head . tail)
  (if (null? head) head
      `(if ,head ,(cons 'and tail) false)))

(defmacro defn (name args . body)
  `(def ,name (lambda ,args ,(cons 'do body))))

(defmacro assert (test)
  `(cond ((not ,test) (error '("assertion failed:" ,test)))))


;; typeclasses
(defmacro class (name args . funcs)
  (do
    
    (defn make-instantiate-from (tc)
      ;; obtain instance from arg list (first arg)
      (lambda (args)
              (get-attr tc (type (car args)))))
    
    (defn make-late-func (name)
      ;; typeclass -> wrapped function
      (lambda (tc)
              (do
                (def instanciate-from (make-instantiate-from tc))
                (lambda args
                        (do 
                          (def tc (instanciate-from args))
                          (def overload (get-attr tc name))
                          (apply overload args))))))

    (def func-defs (list-map funcs (lambda (x)
                                 (do
                                   (def func-name (car x))
                                   (def func-args (cdr x))
                                   (def overload-name func-name) ;;(symbol-append '~ func-name))
                                   (def late-func (make-late-func func-name))

                                   `(def ,overload-name (,late-func ,name)))
                                 )))

    ;; typeclass + overloaded function definitions
    (def tc-def `(def ,name (object 'class)))
    (def defs (list-append (list tc-def)
                      func-defs))
    
    (cons 'do defs)))

                                         
(defmacro instance (c t . body)
  (do
    
    (def instance-def `(make-attr! ,c (quote ,t) (object 'instance)))
    
    (def func-defs (list-map body (lambda (x)
                                     (do
                                       (def name (car x))
                                       (def fun (cadr x))
                                       `(make-attr! (get-attr ,c (quote ,t)) (quote ,name) ,fun)))))
    (list-append (list 'do instance-def) func-defs)))


(class Functor a
       (map a f))

(class Seq a
       (each a f))

(instance Functor list
          (map list-map))

(instance Seq list
          (each list-each))

(class Sum a
       (+ a a))

(class Diff a
       (- a a))

(instance Sum int (+ number-add))
(instance Sum real (+ number-add))
(instance Sum string (+ string-append))
(instance Sum symbol (+ symbol-append))
(instance Sum list (+ list-append))

(instance Diff int (- number-sub))
(instance Diff real (- number-sub))

(class Eq a
       (= a a))

(instance Eq int
          (= number=?))

(instance Eq real
          (= number=?))

(instance Eq string
          (= string=?))

