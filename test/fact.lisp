(def fact (fn (n) (cond
				   ((= n 0) 1)
				   (true (* n (fact (- n 1)))))))
(fact 5)

