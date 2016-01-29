

(defn pred (value)
  (echo value)
  value)

(echo "and: " (and (pred true) (pred false)))
(echo "and: " (and (pred false) (pred true)))

(echo "or: " (or (pred true) (pred false)))
(echo "or: " (or (pred false) (pred true)))


