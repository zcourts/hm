(def not (fn (x) (cond (x false)
                       (true true))))

(defmacro quasiquote (e)
  (do
    (echo ">>" e)
    (def res 
      (cond ((not (list? e)) (list 'quote e))
            ((not (= (length e) 2)) (cons 'list (map e (fn (x) (list 'quasiquote x)))))
            ((not (eq? (nth e 0) 'unquote)) (cons 'list (map e (fn (x) (list 'quasiquote x)))))
            (true (nth e 1))))
    (echo "<<" res)
    res))

(def x 42)
(echo `(michel ,x (y 14 ,x)))






