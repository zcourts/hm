
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
    
    (defn make-instantiate-from (tc error-message)
      ;; obtain instance from arg list (first arg)
      (lambda (args)
              (object-attr tc (type (car args)) error-message)))
    
    (defn make-late-func (f)
      ;; typeclass -> wrapped function
      (lambda (tc)
              (do

                (def type-error (string-append "first argument must be an instance of " (to-string name)))
                (def instantiate-from (make-instantiate-from tc type-error))
                (def func-error (string-append "instance does not implement " (to-string f)))
                (lambda args
                        (do 
                          (def instance (instantiate-from args))
                          (def overload (object-attr instance f func-error))
                          (apply overload args))))))
    (defn make-late-func-closure (f)
      ;; typeclass -> wrapped function
      (lambda (tc)
             (make-overload tc name f)))
    
    (def func-defs (list-map funcs (lambda (x)
                                 (do
                                   (def func-name (car x))
                                   (def func-args (cdr x))
                                   (def overload-name func-name) ;;(symbol-append '~ func-name))
                                   (def late-func (make-late-func-closure func-name))
                                   `(def ,overload-name (,late-func ,name)))
                                 )))

    ;; typeclass + overloaded function definitions
    (def tc-def `(def ,name (object 'class)))
    (def defs (list-append (list tc-def)
                      func-defs))
    
    (cons 'do defs)))


;; TODO check that everything is implemented
(defmacro instance (c t . body)
  (do
    
    (def instance-def `(object-make-attr! ,c (quote ,t) (object 'instance)))
    
    (def func-defs (list-map body (lambda (x)
                                     (do
                                       (def name (car x))
                                       (def fun (cadr x))
                                       `(object-make-attr! (object-attr ,c (quote ,t)) (quote ,name) ,fun)))))
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

(defn != (a b) (not (= a b)))

(instance Eq int
          (= number=?))

(instance Eq real
          (= number=?))

(instance Eq string
          (= string=?))

(class Show a
       (show a))

(instance Show int (show to-string))
(instance Show real (show to-string))
(instance Show bool (show to-string))
(instance Show string (show to-string))
(instance Show list (show to-string))
(instance Show symbol (show to-string))

