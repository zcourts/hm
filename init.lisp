
;; some helpers
(def not (lambda (x) (cond (x false) (true true))))

(def length (lambda (x)
			  (cond ((null? x) 0)
					('else (+ 1 (length (cdr x)))))))

(def cadr (lambda (x) (car (cdr x))))

(def map (lambda (x f)
		   (cond ((null? x) x)
				 ('else (cons (f (car x)) (map (cdr x) f))))))

(def list (lambda args args))



;; the mother of all macros TODO unquote-splicing ?
(defmacro quasiquote (e)
  (cond
    ;; not a list: just quote
    ((not (list? e)) (list 'quote e))
    ;; empty list: as is
    ((null? e) (list 'quote e))

    ;; unquote
    ((eq? (car e) 'unquote)
     (cond ((= (length e) 2) (cadr e))
           ('else (error "bad unquote syntax"))))
    ('else
     ;; (quasiquote (a b c)) => (list (quasiquote a) (quasiquote b) (quasiquote c))
     (cons 'list (map e (lambda (x) (list 'quasiquote x)))))))

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



(echo "lisp interpreter started")
