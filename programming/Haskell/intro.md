# Programming in Haskell 2nd (Graham Hutton)

Maybe I should try split this note into chapters...

# Chapter 1

polymorphic types and type inference

# Chapter 2

## Exercises

```haskell
-- 2.7.4 provide two other implementation to get the last member of a list i.e. make your own prelude `last`

-- linear time recursive definition
last' :: [a] -> a
last' [] = undefined
last' [x] = x
last' (_ : x' : xs) = last' (x' : xs)

-- indexing and function composition
last'' :: [a] -> a
last'' x = ((x !!) . subtract 1 . length) x

fnEq :: (Eq a) => b -> (b -> a) -> (b -> a) -> Bool
fnEq val f g = f val == g val
```

```haskell
-- 2.7.5 provide two other implementation to get all except the last i.e. prelude `init`

-- indexing
init' :: [a] -> [a]
init' xs = map (xs !!) [0 .. length xs - 2]

-- recursion and concatenation
init'' :: [a] -> [a]
init'' [] = undefined
init'' [_] = []
init'' (x : xs) = x : init'' xs
```

## Remarks

[prelude `init`](https://hackage-content.haskell.org/package/ghc-internal-9.1401.0/docs/src/GHC.Internal.List.html#init)

```haskell
init []                 =  errorEmptyList "init"
init (x:xs)             =  init' x xs
  where init' _ []     = []
        init' y (z:zs) = y : init' z zs
```

Note that `init''` after determining `xs` ain't empty and decides to recurse itself, first thing it do is check if the exact same `xs` is empty again, meaning redundant if-else branch, thus hurting performance. The prelude version only checks the cut version, so that saves one if-else per element, meaning the prelude is probably gonna be faster.

### Digging into Assembly

```bash
# llvm IR
$ ghc -O2 -fllvm -keep-llvm-files Wut.hs

# aarch64 assembly
$ llc -march=aarch64 -filetype=obj Wut.ll -o Wut.aarch64.o
$ llvm-objdump -d --arch-name=aarch64 Wut.aarch64.o > Wut.aarch64.S
# alternatively...
$ llc -march=aarch64 Wut.ll -o Wut.aarch64.s

# riscv64 assembly
# mattr and target-abi else llc might complain this:
# LLVM ERROR: GHC calling convention requires the (Zfinx/F) and (Zdinx/D) instruction set extensions
$ llc -march=riscv64 -mattr=+f,+d -target-abi=lp64d -filetype=obj Wut.ll -o Wut.riscv64.o
$ llvm-objdump -d --arch-name=riscv64 Wut.riscv64.o > Wut.riscv64.S
# alternatively...
$ llc -march=riscv64 -mattr=+f,+d Wut.ll -o Wut.riscv64.s
```

[so-on-calling-convention]: https://stackoverflow.com/questions/6394937
[llvm-calling-convention]: https://llvm.org/docs/LangRef.html#calling-conventions

An important [realization][so-on-calling-convention] is that any given architecture may be bestowed upon several calling conventions. In particular, for Haskell/GHC/LLVM on x86-32/x86-64/aarch64/riscv64, _callee save registers_ are [**disabled**][llvm-calling-convention].
In other words, the assembly here does not obey either [RISC-V](https://riscv.org/wp-content/uploads/2024/12/riscv-calling.pdf) [or](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-160#integer-registers) [ARM](https://devblogs.microsoft.com/oldnewthing/20220823-00/?p=107041).

Given that the hardware assembly lacks certain features of CCC (the C Calling Convention), we might be better off checking IR.
The above snippet also gives `Wut.ll` which is some sort of LLVM IR, but TBH it's still intimidating to noobs like me.
GHC has its own sort of IR, though: GHC Core.

```bash
$ ghc -O0 -ddump-simpl -ddump-to-file -dsuppress-all Wut.hs
$ less Wut.dump-simpl
```

```haskell
mine :: [a] -> [a]
mine [] = undefined
mine [_] = []
mine (x : xs) = x : mine xs

preDiy :: [a] -> [a]
preDiy [] = undefined
preDiy (x : xs) = preDDiy x xs
  where
    preDDiy _ [] = []
    preDDiy y (z : zs) = y : preDDiy z zs
```

```haskell
==================== Tidy Core ====================

Result size of Tidy Core
  = {terms: 107, types: 74, coercions: 8, joins: 0/1}

Rec {
mine
  = \ @a_aM2 ds_X1 ->
      case ds_X1 of {
        [] -> undefined ($dIP_rPK `cast` <Co:4> :: ...);
        : ds1_dPt ds2_dPu ->
          case ds2_dPu of wild1_X3 {
            [] -> [];
            : ipv_sPA ipv1_sPB -> : ds1_dPt (mine wild1_X3)
          }
      }
end Rec }
```

```haskell
preDiy
  = \ @a_ay5 ds_X1 ->
      case ds_X1 of {
        [] -> undefined ($dIP1_rPL `cast` <Co:4> :: ...);
        : x_axt xs_axu ->
          letrec {
            preDDiy_aLa
              = \ ds1_dPe ds2_dPf ->
                  case ds2_dPf of {
                    [] -> [];
                    : z_axx zs_axy -> : ds1_dPe (preDDiy_aLa z_axx zs_axy)
                  }; } in
          preDDiy_aLa x_axt xs_axu
      }
```

You might want to look into [`@` for patter binding (yeah just like Rust!) aka _as-pattern_ and _`case` expressions_ which also accepts patterns](https://www.haskell.org/tutorial/patterns.html), [how baskslash `\` makes another way defining a function](https://stackoverflow.com/a/34794297/9933842), and [how we get from `Bind::Rec` to `Rec {` and `end Rec }`](https://gitlab.haskell.org/ghc/ghc/-/blob/master/compiler/GHC/Core/Ppr.hs).

So yes, at least in the GHC Core stage, the prelude version does recurse with the truncated version of the slice, whereas our DIY version recurses with a version which is guaranteed not to be empty, only to check if it's empty right in the first step into the recursion.

I'm wondering if that's avoidable in the final assembly, though... maybe a sufficiently optimizing compiler might be able to pull that off?

N.B. much is yet to be fact check and organized from [AI slop](./unorganized-gemini-slop-ch2.md), but here are (not rendered) some links to key ideas:

[flags-on-emitting-ghc-core]: https://ghc.gitlab.haskell.org/ghc/doc/users_guide/debugging.html
[ghc-core-syntax]: https://gitlab.haskell.org/ghc/ghc/-/wikis/commentary/compiler/core-syn-type
[system-fc]: https://gitlab.haskell.org/ghc/ghc/-/wikis/commentary/compiler/fc
[ghc-primitive-types]: https://ghc.gitlab.haskell.org/ghc/doc/users_guide/exts/primitives.html
[ghc-sigil-hash]: https://ghc.gitlab.haskell.org/ghc/doc/users_guide/exts/magic_hash.html
[ghc-join]: https://www.google.com/search?q=https://gitlab.haskell.org/ghc/ghc/-/wikis/commentary/compiler/join-points
[ghc-sans-continuations]: https://www.microsoft.com/en-us/research/wp-content/uploads/2016/11/join-points-pldi17.pdf
[ghc-stg]: https://gitlab.haskell.org/ghc/ghc/-/wikis/commentary/compiler/generated-code

# Quirks

## Operators

- `/=`: Not equal
- `not`: Not
- `succ`: successor
- `fst`: first member of tuple
- `snd`: second member of tuple
- `head`/`tail`: the first and remaining parts of a list
- `init`/`last`: the prefixes and the last part of a list
- `maximum`/`minimum`/`sum`/`product`/`reverse`/`length`: apply to lists
- `take`: `Iterator::take`
- `drop`: `Iterator::skip`
- `null`: `Vec::is_empty`
- `++`: concat
- `!!`: `slice::get`, 0-indexed
- `div`: a _function_ with two arguments, often used in the infix operator form e.g. ``x `div` y`` meaning $\frac{x}{y}$.

### Change Parser Behavior: `$` and `.`

[What is the difference between . (dot) and $ (dollar sign)?](https://stackoverflow.com/questions/940382)

`$` is a bit like "hey I knew Haskell juxtaposition binds tightly, but would you not?", sort of like replacing `$` with a `(`, also inserting a pairing `)` after whatever on the right hand side forms a valid expression.

```haskell
putStrLn (show (1 + 1))
-- this clearly does not work...
-- putStrLn (show 1 + 1)
putStrLn (show $ 1 + 1) -- stop `show` from binding to `1`, so `1 + 1` is "tighter"
putStrLn $ show (1 + 1) -- similaly, `putStrLn` accepts one argument: not `show`, but `show (1 + 1)`
putStrLn $ show $ 1 + 1
```

`.` is like function compisition, so $f(g(x))$ sort of thing.

```haskell
(putStrLn . show) (1 + 1) -- N.B. the parenthesis around the result of function composition is required
putStrLn . show $ 1 + 1
```

[See also](https://stackoverflow.com/questions/631284)

### [Precedence](https://rosettacode.org/wiki/Operator_precedence#Haskell)

When using functions as infix operators via ` `` `, they have quite low precedence e.g. lower than `++`, for example these two are equal:

```haskell
tr chars = [ch | ch <- chars, ch `elem` ['a'..'z'] ++ ['A'..'Z']]
tr' chars = [ch | ch <- chars, ch `elem` (['a'..'z'] ++ ['A'..'Z'])]
```

## Type Class

[`Int` is type, `Integral` is type class](https://stackoverflow.com/questions/63818160)

# Questions

- Using `$` with functions that accepts multiple parameters?
- What the hell are actions, functors, and monads anyway?

# Examples

```haskell
handshakes :: [a] -> [(a, a)]
handshakes [] = []
handshakes (x : ys) = handshakes ys ++ zip ys (repeat x)

handshakes' :: (Integral a) => a -> [(a, a)]
handshakes' 0 = []
handshakes' x = (handshakes . take (fromIntegral x) . iterate (+ 1)) 1

handshakes'' n = [ (a,b) | a <- [1..n-1], b <- [a+1..n] ]
```

# References

[basics]: https://www.cs.columbia.edu/~sedwards/classes/2019/4995-fall/basics.pdf
[haskell-as-rust-ffi]: https://stackoverflow.com/questions/31196449
