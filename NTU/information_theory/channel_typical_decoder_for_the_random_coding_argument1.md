---
tags: information theory, linear algebra, probability, channel coding, random coding argument
---

Warning: Latex ahead. Here's <a href="https://hackmd.io/nEcnq3WrQeuvLc-rrbAXwA">hackmd</a> for better rendering.

In "Unit2_ho.pdf", page 55, we discussed how to define a decoder based on typicality. Recap in my own words as follows: suppose $D$ is the decoder function:$$
D: Y^n \to W^k ,~ D(y^n) = w \\
\text{if there exists codebook } c \text{ such that }( c(w)=x^n, y^n ) \in \mathcal{T}_\varepsilon^{(n)}{ \left( P_{X,Y} \right) }
$$

My question is, what if for some $y^n \in Y^n$ there's *no* message $w \in W^k$ and/or codebook $c$ such that $c$ maps $w$ to $x^n$ and $(x^n, y^n) \in \mathcal{T}_\varepsilon^{(n)}$? What should the decoder map $y^n$ to?

Or more fundamentally, with the symmetricity of rows of random codebooks in mind, for all $w\in W^k$, we should have *exactly the same distribution $P_{X^n, Y^n}$* over the Cartesian product $X^n \times Y^n$, i.e. for any channel input/output sequence pair $(x^n, y^n)$, the probability that message $w=i$ is to be this pair would be equal to the probability that message $w=j$ is to have this pair, for any $(i,j)$, which means for a given $y^n \in Y^n$, if there's $x^n \in X^n$ making the pair $(x^n, y^n) \in \mathcal{T}_\varepsilon^{(n)}$, *all message $w \in W^k$ can be chosen as decoder output*. In that case, there shall be no way the error probability is to approach $0$.

What's going wrong?