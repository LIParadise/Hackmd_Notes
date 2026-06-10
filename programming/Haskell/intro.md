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

You might want to look into [`@` for pattern binding (just like Rust!) aka _as-pattern_ and _`case` expressions_ which also accepts patterns](https://www.haskell.org/tutorial/patterns.html), [how backslash `\` makes lambda functions][haskell-lambda-explained], and [how we get from `Bind::Rec` to `Rec {` and `end Rec }`](https://gitlab.haskell.org/ghc/ghc/-/blob/master/compiler/GHC/Core/Ppr.hs).

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
[haskell-lambda-explained]: https://stackoverflow.com/a/34794297/9933842

# Chapter 3

## Currying

Note that function definitions are essentially _giving names to (anonymous) lambdas_ modulo _pattern matching_, so conceptually we may translate

```haskell
zip' :: [a] -> [b] -> [(a, b)]
zip' []     _      = []
zip' _      []     = []
zip' (x:xs) (y:ys) = (x, y) : zip' xs ys
```

into some (pseudo) Rust code like this

```rust
match &lhs {
    [] => |_| { [] },
    [l, lhs@..] => |rhs| match &rhs {
        [] => [],
        [r, rhs@..] => vec![(l, r)].append(&mut (zip_prime(lhs))(rhs))
    }
}
```

and with some lifetime bypassing with `Box<dyn Fn>` (could you think of more efficient way? since this allocates a _lot_...; maybe worker functions...? but denoting lifetime with lambdas is still [rfc](https://internals.rust-lang.org/t/pre-rfc-allow-for-a-syntax-with-closures-for-explicit-higher-ranked-lifetimes/15888)...), and you cannot implement your own [functor like you do with operator `()` overloading in C++ in 2026 Rust 1.96](https://github.com/rust-lang/rust/issues/29625).

```rust
fn zip_prime<T: Copy, U: Copy>(
    lhs: &[T],
) -> Box<dyn Fn(&[U]) -> Vec<(T, U)> + '_> {
    match lhs {
        [] => Box::new(|_| Vec::new()),
        [l, lhs @ ..] => Box::new(move |rhs| match rhs {
            [] => Vec::new(),
            [r, rhs @ ..] => [(*l, *r)]
                .into_iter()
                .chain((zip_prime(lhs))(rhs))
                .collect(),
        }),
    }
}
```

Here's a way to think about curried functions: consider only the types. So `zip' :: [a] -> [b] -> [(a, b)]`: when we say `zip' xs ys`, we first note that `zip' xs` itself is an _expression_ (?) of a _function_ (remember that function calls are of highest precedence and binds to left?), of which type is `[b] -> [(a, b)]`, so when we then say `zip' xs ys`, the function `zip' xs` eats `ys` so overall the expression (?) yields a `[(a, b)]`.

So we have the type, and correspondingly how to mentally parse the function call expression of curried functions, figured out, now we may think about its body.
Welp in this case it concatenate two lists, one of which is (recursively) calling "itself".
That's it.

```rust
fn zip_prime_vec_deque<T, U>(lhs: Vec<T>) -> impl FnOnce(Vec<U>) -> Vec<(T, U)> {
    use std::collections::VecDeque;
    fn zip_prime_vec_deque_inner<T, U>(
        mut lhs: VecDeque<T>,
    ) -> impl FnOnce(VecDeque<U>) -> VecDeque<(T, U)> {
        move |mut rhs: VecDeque<U>| {
            if let Some(l) = lhs.pop_front() {
                if let Some(r) = rhs.pop_front() {
                    std::iter::once((l, r))
                        .chain((zip_prime_vec_deque_inner(lhs))(rhs))
                        .collect()
                } else {
                    VecDeque::new()
                }
            } else {
                VecDeque::new()
            }
        }
    }
    move |rhs: Vec<U>| {
        Vec::from((zip_prime_vec_deque_inner(VecDeque::from(lhs)))(
            VecDeque::from(rhs),
        ))
    }
}
```

```rust
/// XXX:
/// O(max(n, m)) and allocates a lot due to the [`std::iter::FromIterator`] calls
fn zip_prime_linear_max_fn_once<T, U>(mut lhs: Vec<T>) -> impl FnOnce(Vec<U>) -> Vec<(T, U)> {
    lhs.reverse();
    fn zip_back<T, U>(mut lhs: Vec<T>) -> impl FnOnce(Vec<U>) -> Vec<(T, U)> {
        move |mut rhs: Vec<U>| {
            if let Some(l) = lhs.pop() {
                if let Some(r) = rhs.pop() {
                    std::iter::once((l, r))
                        .chain((zip_back(lhs))(rhs))
                        .collect()
                } else {
                    vec![]
                }
            } else {
                vec![]
            }
        }
    }
    move |mut rhs: Vec<U>| {
        rhs.reverse();
        Vec::from((zip_back(lhs))(rhs))
    }
}
```

```rust
/// It might be interesting to be even more lazier,
/// in the sense return [`Iterator`] instead of allocate,
/// but see also https://github.com/rust-lang/rust/issues/99697
fn zip_prime_clone_fn_once<T: Clone, U: Clone>(lhs: &[T]) -> impl FnOnce(&[U]) -> Vec<(T, U)> + '_ {
    /// Is pre-allocate like this considered functional programming?
    /// If not, how to pre-allocate in pure FP?
    fn zip_prime_clone_fn_once_inner<T: Clone, U: Clone>(
        mut ret: Vec<(T, U)>,
        lhs: &[T],
    ) -> impl FnOnce(&[U]) -> Vec<(T, U)> {
        move |rhs: &[U]| match (lhs, rhs) {
            ([], _) | (_, []) => ret,
            ([l, lhs @ ..], [r, rhs @ ..]) => {
                ret.push((T::clone(l), U::clone(r)));
                (zip_prime_clone_fn_once_inner(ret, lhs))(rhs)
            }
        }
    }
    move |rhs: &[U]| {
        let ret = Vec::with_capacity(std::cmp::min(lhs.len(), rhs.len()));
        (zip_prime_clone_fn_once_inner(ret, lhs))(rhs)
    }
}
```

## Class Constraint and Polymorphic

> A type that contains _one or more type variables_ is called _**polymorphic**_ ("of many forms"), as is an expression with such a type. Hence, `[a] -> Int` is a _polymorphic type_ and `length` is a _polymorphic function_. More generally, many of the functions provided in the standard prelude are polymorphic.

> The idea that `+` can be applied to numbers of any numeric type is made precise in its type by the inclusion of a _class constraint_. Class constraints are written in the form `C a`, where `C` is the name of a class and `a` is a type variable.
> function `(+)` has type `a -> a -> a`. (Parenthesising an operator converts it into a curried function, as we shall see in chapter 4.)

> A type that contains _one or more class constraints_ is called _**overloaded**_, as is an expression with such a type.
> Hence, `Num a => a -> a -> a` is an _overloaded type_ and `(+)` is an _overloaded function_.

versus [ad-hoc](https://wiki.haskell.org/index.php?title=Polymorphism&oldid=59216) [polymorphism](https://www.reddit.com/r/haskell/comments/5mbblu/comment/dc29thl)...?

> Numbers themselves are also overloaded. For example, `3 :: Num a => a` means that for any numeric type `a`, the value `3` has type `a`. In this manner, the value `3` could be an integer, a floating-point number, or more generally a value of any numeric type, depending on the context in which it is used.

So _overloading_. In C++, it's specifying what different types, or different type classes in Haskell terms if we consider C++ inheritance sort of type classes, to have different behaviors despite the fact that the code "look the same". In other words, a compile time trick to write the same code for some set of types allowing them to have different behaviors.

```cpp
#include <cstdint>
#include <iostream>
#include <typeinfo>

struct A {
        int32_t operator()(int32_t i) const {
            std::cout << "A::()(int32_t)" << std::endl;
            return i + 1;
        }
        u_int8_t operator()(u_int8_t u) const {
            std::cout << "A::()(u_int8_t)" << std::endl;
            return u + 1;
        }
};
class B : public A {};

void foo(const B &b) { std::cout << "B" << std::endl; }
void foo(const A &b) { std::cout << "A" << std::endl; }

int main() {
    const A &a = {};
    const B &b = {};
    foo(a);
    foo(b);
    foo(static_cast<const A &>(b));

    auto a_3 = a(3);
    auto b_4u = b((u_int8_t)4);
    auto a_4i8 = a((int8_t)4);
    std::cout << typeid(a_3).name() << std::endl;
    std::cout << typeid(b_4u).name() << std::endl;
    std::cout << typeid(a_4i8).name() << std::endl;

    return 0;
}
```

So in [Haskell](https://www.haskell.org/tutorial/classes.html), things like `wut :: (Num a) => a -> a -> [a]`, are ways to implement such overloading behavior, thus we mention them as _overloaded types_.
In other words, we expect the (concrete) types in certain _type class_ to all have certain "behaviors" - operators or functions we may invoke - and we do expect the implementation to be somewhat arbitrary.
So [very similar to (but more powerful in some way than) Rust traits](https://www.reddit.com/r/rust/comments/1e0fuon/comment/lcmljta), essentially.

```haskell
class Eq a where
  (==)                  :: a -> a -> Bool

instance Eq Integer where
  x == y                =  x `integerEq` y

instance Eq Float where
  x == y                =  x `floatEq` y

instance (Eq a) => Eq (Tree a) where
  Leaf a         == Leaf b          =  a == b
  (Branch l1 r1) == (Branch l2 r2)  =  (l1==l2) && (r1==r2)
  _              == _               =  False
```

```rust
trait Foo {
    fn foo(&self);
}
struct Bar;
struct Baz;
impl Foo for Bar {
    fn foo(&self) {
        println!("{}", std::any::type_name::<Self>());
    }
}
impl Foo for Baz {
    fn foo(&self) {
        println!("{}", std::any::type_name::<Self>());
    }
}
```

> In this sense, we expect `==` to be overloaded to carry on these various tasks.
> Type classes conveniently solve both of these problems. They allow us to declare which types are instances of which class, and to provide definitions of the overloaded operations associated with a class.

There is a related trick in Rust: you may define `std::ops::Add<T>` for multiple _concrete_ `T`, thus overloading operator `+` (interesting to note that you cannot do this to `Deref` since in this case there's only one `Deref` trait and the target specified by `<Ptr as Deref>::Target`; [higher kinded types](https://news.ycombinator.com/item?id=17537980)?) in more of C++ sense.

```rust
struct App;
impl std::ops::Add<&str> for &App {
    type Output = String;
    fn add(self, rhs: &str) -> Self::Output {
        String::from_iter([rhs, "hehe"])
    }
}
impl std::ops::Add<u32> for &App {
    type Output = u32;
    fn add(self, _: u32) -> Self::Output {
        67
    }
}
fn hehe() {
    const APP: &App = &App;
    println!("{:?}", APP + "wut");
    println!("{:?}", APP + 114514);
}
```

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
