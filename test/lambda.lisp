(def add (fn (x)
           (fn (y)
             (+ x y) )))


(echo ((add 2) 3))
(echo (map '(1 2 3) (add 2)))


