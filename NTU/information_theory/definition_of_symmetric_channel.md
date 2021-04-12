---
tags: information theory, linear algebra, probability, symmetric channel
---


Alert: many Latex ahead. Here's a <a href="https://hackmd.io/AISqCBRlTne6CN_vS0ZcBA">hackmd.io link</a> for better rendering.

In slide "Unit2_h0.pdf" around page 37, we discussed how to generalize the concept of symmetric channel beyond binary case, i.e. when can we say that $C = \log{\mathcal{Y}} - H(p)$, where $p$ have same distribution as rows in matrix $P_{Y|X}$. We proposed 2 criterion, and later used them to define symmetric channel. My question is, can we construct a DMC w/o feedback channel where it *behaves* like a symmetric channel, but don't follow the 2 criterions?

Suppose $p, q, u$ be $1 \times n$ probability vector, where $u$ is uniform, i.e. $u_i = \frac{1}{n} ~\forall 1 \leq i \leq n ~,~ n \in \mathbb{N}$, $A$ be a $n \times n$ matrix with rows being permutation of $p$. We ask:

Given that $q \times A = u$, is it guaranteed that $q = u$?

If this proposition doesn't hold, the counter example would corresponds to a channel law for DMC w/o feedback having channel capacity the same form as symmetric channels without following the 2 criterions: let $P_X = q, P_{Y|X} = A, P_Y = u$. In other words, there would be a channel law which don't fall into the definition of symmetric channel, at least by the definition in the slides, nevertheless have same form of channel capacity.

I can't quite construct such an example, though. Here's a failed try: <a href="https://matrixcalc.org/en/#%7B%7B0%2e2,0%2e3,0%2e1,0%2e4%7D%7D%2a%7B%7B0%2e1,0%2e2,0%2e3+2/30,1/3%7D,%7B0%2e2,0%2e1,1/3,0%2e3+2/30%7D,%7B0%2e3+2/30,1/3,0%2e1,0%2e2%7D,%7B1/3,0%2e3+2/30,0%2e2,0%2e1%7D%7D">matrixcalc.org link</a>, but I can't either prove the aforementioned proposition.