
(defn fib (n)
  (cond
   ((= n 0) 0)
   ((= n 1) 1)
   ('else
	(+ (fib (- n 1)) (fib (- n 2))))))


(def fib2 (lambda (n)
			(cond
			 ((number=? n 0 ) 0)
			 ((number=? n 1 ) 1)
			 ('else
			  (number-add (fib2 (number-sub n 1)) (fib2 (number-sub n 2)))))))

(fib 25)
