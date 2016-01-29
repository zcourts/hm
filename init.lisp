

(def not (fn (x) (cond (x false) (true true))))

;; the mother of all macros TODO unquote-splicing ?
(defmacro quasiquote (e)
  (cond
    ;; not a list: just quote
    ((not (list? e)) (list 'quote e))
    ;; empty list: as is
    ((= (length e) 0) e)

    ;; unquote
    ((eq? (nth e 0) 'unquote)
     (cond ((= (length e) 2) (nth e 1))
           ('else (error "bad unquote syntax"))))
    ('else
     ;; (quasiquote (a b c)) => (list (quasiquote a) (quasiquote b) (quasiquote c))
     (cons 'list (map e (fn (x) (list 'quasiquote x)))))))

(defmacro defn (name args . body)
  `(def ,name (fn ,args ,(cons 'do body))))


(echo "lisp interpreter started")
