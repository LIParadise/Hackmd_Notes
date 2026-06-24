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

> There is another kind called [ad hoc polymorphism](https://www.haskell.org/tutorial/classes.html), better known as _overloading_

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

In [Haskell](https://www.haskell.org/tutorial/classes.html), things like `wut :: (Num a) => a -> a -> [a]`, are ways to implement such overloading behavior, thus we mention them as _overloaded types_.
In other words, we expect the (concrete) types in certain _type class_ to all have certain "behaviors" - operators or functions we may invoke - and we do expect the exact implementations to be somewhat arbitrary.
So [very similar to (but more powerful in some way than) Rust traits](https://www.reddit.com/r/rust/comments/1e0fuon/comment/lcmljta), essentially.

In other words, if any type suffices, it's _polymorphic_. If only types in certain type classes would do, we most likely want to endow tailored behaviors depending on the actual type, meaning it's _overloaded_ i.e. _ad hoc polymorphism_.

> In this sense, we expect `==` to be overloaded to carry on these various tasks.

```haskell
class Eq a where
  (==)                  :: a -> a -> Bool
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

> Note that these overloaded behaviors are different for each type (in fact the behavior is sometimes undefined, or error), whereas in parametric polymorphism the type truly does not matter (`fringe`, for example, really doesn't care what kind of elements are found in the leaves of a tree). In Haskell, type classes provide a structured way to control ad hoc polymorphism, or overloading.
> Type classes conveniently solve both of these problems. They allow us to declare which types are instances of which class, and to provide definitions of the overloaded operations associated with a class.

To reiterate, essentially, we want _number literals_, albeit not all numbers are equal (fixed/variable length integers, for example); we want operators on numbers, but we gotta have a different set of operations for each kind of numbers; we want `==`, but not all types are compared in the same way, e.g. lists delegate to the values (let us not consider pointer/referential equality in Haskell), while equality for functions are non-trivial. These are all _overloading_ i.e. _ad-hoc polymorphicsm_.

> a type `a` is an _instance_ of the _class `Eq`_ if there is an (overloaded) operation `==`, of the appropriate type, defined on it.
> For every type `a` that is an _instance_ of the _class `Eq`_, `==` has type `a -> a -> Bool`

Any type with specific behaviors defined may be encompassed by certain type classes, and for any type in the given type class, we know there's (overloaded to meet their needs) certain behaviors that we may invoke.
Again, a lot like Rust traits.

> `Eq a` is not a type expression, but rather it expresses a _constraint on a type_, and is called a _**context**_.
> _Contexts_ are placed at the front of _type expressions_.

> But how do we specify which types are _instances_ of the class `Eq`, and the actual behavior of `==` on each of those types? This is done with an _instance declaration_.
> The definition of `==` is called a _method_.

```haskell
instance Eq Integer where
  x == y                =  x `integerEq` y

instance Eq Float where
  x == y                =  x `floatEq` y

instance (Eq a) => Eq (Tree a) where
  Leaf a         == Leaf b          =  a == b
  (Branch l1 r1) == (Branch l2 r2)  =  (l1==l2) && (r1==r2)
  _              == _               =  False
```

> Class methods may have additional class constraints on any type variable except the one defining the current class.

```haskell
class C a where
  m                     :: Show b => a -> b
```

In Rust, besides generics inside traits, a relevant trick is that you may define `std::ops::Add<T>` for multiple _concrete_ `T`, thus overloading operator `+` (interesting to note that you cannot do this to `Deref` since in this case there's only one `Deref` trait and the target specified by `<Ptr as Deref>::Target`; [higher kinded types](https://news.ycombinator.com/item?id=17537980)?), arguably in a more of C++ way.

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

### Unfinished Thoughts

Functions eat values (of certain type) and returns values (of yet another certain type).
Type constructors eat types and return types.

In Rust terms, Haskell `Functor`/`fmap` is describing certain sort of generic types `struct Foo<T>` to which we can write _functions_ that map `Foo<T>` to values of `Foo<U>`, given we have a function mapping `T` to `U`, i.e. `fmap(<U as From<T>>::from)` is itself a function s.t. `(fmap(<U as From<T>>::from)) (value_foo_t)` yields `(value_foo_u)`.

It's interesting to see the notations kinda mixed together when we think about pattern matching:

```Haskell
class Functor generic_type where
  fmap                  :: (a -> b) -> generic_type a -> generic_type b -- both `generic_type a` and `generic_type b` are types

instance Functor Tree where
  fmap map_a_to_b (Leaf x)       = Leaf   (map_a_to_b x) -- `x` is value of type `a`, `map_a_to_b x` is value of type `b`
  fmap map_a_to_b (Branch t1 t2) = Branch (fmap map_a_to_b t1) (fmap map_a_to_b t2)
```

Maybe we can think about `Functor`/`fmap` this way: a type is some set of values, and functions `a -> b` are some concrete mapping between sets $A$ and $B$.
So if we have projections, mapped image, or whatever set that looks like $A$, say $G_A$, and similar approach may be applied to $B$ to get $G_B$, we would like a function that maps $G_A$ to $G_B$.

Consider function $f = \lfloor \cdot \rfloor: \mathbb{R} -> \mathbb{Z}$. Then an example of `fmap f` may be a function that maps $\mathbb{R}^3$ to $\mathbb{Z}^3$, here $G$/`g` is the act of taking $3$ (may repeat) elements from certain set to form a vector/list/tuple, and `instance Functor g` is the trivial straightforward element-wise application.

Type constructors eats types and spit out types. Generic types `struct Foo<T>` (in Rust terms) eat one type. But you can eat _more_ types in a curried way: `->` is a type constructor in Haskell, for `a -> b` denotes a function type, equivalently `(->) a b`. Similarly, `==` is a type constructor that takes one type, and `fmap` is a type constructor that takes two ("ordinary") types plus one (higher-ordered) type, i.e. you need `a`/`b`/`g` to describe a function that maps `g a` to `g b`.

Given type classes are more or less traits in Rust, how do we describe Haskell `Functor` in Rust...?
At first glance it seems a bit difficult, since traits in Rust describes collection of first-order types: you can only implement traits for concrete types, no...?

`Integer` is of _kind_ `*`. Types in the type class `Functor` have _kind_ `* -> *`.
Can a type be described by more than one kind? E.g. can a type be both `* -> *` and `* -> * -> *`?

What are _not_ functors? A binary tree is a functor for given function that maps $\mathbb{R}$ to $\mathbb{Z}$ we may trivially define its counterpart mapping binary trees of $\mathbb{R}$ to binary trees of $\mathbb{Z}$ by node-wise applying the original function.
So we have pair of sets $\mathbb{R}$ and $\mathbb{Z}$ and another pair of sets $B \mathbb{R}$ and $B \mathbb{Z}$. For any function $f: \mathbb{R} \to \mathbb{Z}$ we have $f^\prime: (B \mathbb{R}) \to (B \mathbb{Z})$.
$3_\mathbb{R}$ is a set - a set of sequences of $\mathbb{Q}$, an equivalent class defined by Cauchy sequence. Similar to $1_\mathbb{R}$. We certainly have some functions that map $3_\mathbb{R}$ to $1_\mathbb{R}$, e.g. consider element-wise division by $3_\mathbb{Q}$ for sequences in the equivalent class.
$3_\mathbb{Z} = \{ 0, 1, 2 \}$ is a set, so is $1_\mathbb{Z}$. Maybe we can find a mapping between these two sets for every function that maps $3_\mathbb{R}$ to $1_\mathbb{R}$. The floor function brings $3_\mathbb{R}$ to $3_\mathbb{Z}$ and $1_\mathbb{R}$ to $1_\mathbb{N}$. So floor is a functor...?

```
:i Monad
:i Applicative
:i Functor
```

It seems `Functor`/`Applicative`/`Monad` are closely related...

## Ch3 Exercises

- 3.11.5 Equality between functions
  - [Equality](https://math.stackexchange.com/questions/4410882) [between](https://math.stackexchange.com/questions/143727) real numbers is undecidable thanks to [Richardson](https://dl.acm.org/doi/10.1145/190347.190429): though real closed fields are [decidable](https://math.stackexchange.com/questions/151000) - whatever that means, with some non-trivial operators like introduction of $\ln 2$/$\pi$, etc, it's undecidable to determine if two functions over the reals are the same.
  - (IMHO) given a input and function `f` never halts while `g` halts and yield, say `True`, then certainly `f` and `g` cannot be assigned equal. So in general function equality is halting problem and thus undecidable.
  - Formally speaking, by Rice's theorem, all non-trivial properties about Turing machines are undecidable, in particular equivalence between Turing machines. You may be interested in lectures on theory of computation.

# Chapter 4

Note that pattern matching does not require `Eq`: you may define a new `data` without `instance Eq` for it and still be able to pattern destruct them, [just like Rust except Rust allows certain `PartialOrd`: `StructuralPartialEq`](https://doc.rust-lang.org/1.96.0/std/marker/trait.StructuralPartialEq.html).

> In general, if `#` is an operator, then expressions of the form `(#)`, `(x #)`, and `(# y)` for arguments `x` and `y` are called _**sections**_, whose meaning as functions can be formalised using lambda expressions

Note in particular that the _section_ notation respects the LHS/RHS distinction.

```
(#) = \x -> (\y -> x # y)
(x #) = \y -> x # y
(# y) = \x -> x # y
```

> sections are _necessary_ when stating the type of operators
> sections are also necessary when using operators as arguments to other functions

```haskell
import qualified Data.List
(+) :: Int -> Int -> Int
print $ Data.List.foldl' (+) 0 ([1, 3, 5]::[Int])
```

Note also that `data` also introduce new _functions_ (note that the ordering of _records_ matter). OTOH `Foo2` and `Maybe` are _type constructors_: though their kind differs.

```haskell
data Foo2 = Bar2 | Baz2 {bazNumber::Int, bazName::String}
```

```ghci
ghci> :k Foo2
Foo2 :: *
ghci> :k Baz2
Baz2 :: Int -> String -> Foo2
ghci> :t Bar2
Bar2 :: Foo2

ghci> :k Maybe
Maybe :: * -> *
ghci> :t Just
Just :: a -> Maybe a

ghci> :t Foo2
<interactive>:1:1: error: [GHC-01928]
    • Illegal term-level use of the type constructor ‘Foo2’
    • defined at <interactive>:1:1
    • In the expression: Foo2

ghci> :t Maybe
<interactive>:1:1: error: [GHC-01928]
    • Illegal term-level use of the type constructor ‘Maybe’
    • imported from ‘Prelude’
      (and originally defined in ‘GHC.Internal.Maybe’)
    • Perhaps use variable ‘maybe’ (imported from Prelude)
    • In the expression: Maybe
```

See also [wiki book on patterns (specifically _list comprehension_ and _do blocks_)](https://en.wikibooks.org/wiki/Haskell/Pattern_matching#List_comprehensions).

## `foldl` and `foldr`

`foldl` is saying I got a _left-associative_ operator, a seed, and an indexed sequence. One of the straightforward way to see how they interact is imaging you place the seed on the _left most position_ before the sequence and they "snap together" one by one.
`foldr` is the same except it's _right-associative_ operator so you place the seed on the _right most position_.

Here's toy implementation:

```haskell
myFoldl :: (b -> a -> b) -> b -> [a] -> b
myFoldl _ seed [] = seed
myFoldl f seed (x: xs) = myFoldl f (f seed x) xs

myFoldr :: (a -> b -> b) -> b -> [a] -> b
myFoldr _ seed [] = seed
myFoldr f seed (x: xs) = f x $ myFoldr f seed xs
```

I dunno nothing about WHNF, thunk, and redexes though, so the explaination, in particular which part are derived from Haskell semantics and which part are GHC specific behavior are yet to be figured out: take a grain of salt.

Still, we can see why `foldl` tend _not_ to work well with infinite lists: given an infinite list, if we're to "resolve" the value, we're perpetually stuck with `foldl` itself as the outer most pattern: we have no choice but allocate yet another "unevaluated" _thunk_ that's `foldl` (or at least that's what I assume GHC does). And this is exactly why neither `foldl` nor `foldl'` produces a value when the input list is infinite, even though the supplied function might not be _strict_ or simply does not care at all: we _always_ got another `foldl`/`foldl'` to match against.

OTOH on evaluating `foldr` patterns, we see that it immediately exposes the supplied function right at the front, s.t. when we try to "evaluate" the value, we're alternating between the patterns of the supplied function and the patterns of `foldr` itself: we get a chance to actually _short-circuit_ if the function does not care its second argument (i.e. the `foldr` of the rest of the list), thus being able to work with infinite lists. In particular, if list is indeed infinite in length, the supplied seed is never ever actually evaluated, and thus may be `undefined` - in fact if you have a function `foo` that `foldr` with `bar` and supposedly `foo` shall only be called with infinite lists, it's somewhat idiomatic to just say `foo = foldr bar undefined`.

I guess - so again this is high in sodium - since all the strictness and laziness definition of the language, we're essentially parsing the AST before being able to evaluate anything. So in AST terms, `foldl` looks something like this:

```
  foldl
 /  |  \
f  seed [x0, x1, ...]
```

```
  foldl
 /  |  \
f   f   [x1, x2, ...]
   /  \
 seed  x0
```

```
   foldl
  /  |  \
 f   f   [x2, x3, ...]
    /  \
   f    x1
  /  \
seed  x0
```

And while `foldl'` helps somewhat, in using `seq` to make the tree a bit more shallow (and thus avoiding stack overflow when the list is finite but long i.e. when we can actually get rid of the root `foldl`), we're always dealing with _thunk_ that's `foldl` and thus never stops when input list is long.

In short, the difference boils down to these pseudo code as documented by [hoogle](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Prelude.html#v:foldl): note the functions are tagged with sort of timestamp when they got introduced if we were to "evaluate" the value:

- `foldl f seed [x0, x1, x2, ..., xn] = fn( f_n_1( f_n_2( ... f3( f2( f1( f0( seed, x0 )))))))`
- `foldr f seed [x0, x1, x2, ..., xn] = f0(x0, f1(x1, f2(x2, ... f_n_1(x_n_1, f_n(x_n, seed)))))`

This also explains why some deem `foldl` as _reverse_ and `foldr` as _forward_: when you write the semantics out, you can see that `foldr` has an increasing tokens of `f_i`/`x_i` (which also explains why `foldr (:) [] xs = const xs` and people say they apply to the _inside_), and that `foldl` has an _decreasing_ sequence of tokens `f_i`/`x_i` (which also explains why `foldl' (flip (:)) [] xs = reverse xs` and people say they apply to the _outside_): it's the associativity of the function/operator, leading to where we put the seed, and thus leading to how we shall parse the (conceptually) AST and how GHC evaluates them.

Both functions are _lazy_, though: you can say `let x = foldl (flip (:)) [] [1..]` without any issue if you do not try to "evaluate" `x`.

[SO](https://stackoverflow.com/a/3085516/9933842)
[Haskell Wiki](http://www.haskell.org/haskellwiki/Foldr_Foldl_Foldl')

## Ch4 Questions

- How to lambda with patterns? What about if-else or guarded equations?
  - ```haskell
    print $
      map
        ( \xs -> case xs of
            0 -> "foo"
            1 -> "bar"
            _ -> "baz"
        )
        ([0 .. 3] :: [Int])
    ```
  - See also `{-# LANGUAGE LambdaCase #-}` which simplifies `\xs -> case xs of` to simply `\case`
- How does GHC know that we've exhausted the possible patterns?
  - Delegate to primitive types which are a bit like a gigantic enum...? How about Rust?
- Why the heck this compiles (and throws `<<loop>>` during execution)?
  - ```haskell
    mergeSort :: (Ord a) => [a] -> [a]
    mergeSort xs =
      let (lhs, rhs) = splitViaTortoiseHare xs xs
       in do
            let sortedLhs = mergeSort sortedLhs -- WTF?!
            let sortedRhs = mergeSort sortedRhs -- WTF?!
            merge sortedLhs sortedRhs
      where
        splitViaTortoiseHare :: [a] -> [a] -> ([a], [a])
        splitViaTortoiseHare tortoises [] = ([], tortoises)
        splitViaTortoiseHare tortoises [_] = ([], tortoises)
        splitViaTortoiseHare (t : tortoises) (_ : _ : hares) = (\(ts, hs) -> (t : ts, hs)) $ splitViaTortoiseHare tortoises hares
        splitViaTortoiseHare _ _ = error "unreachable"
        merge :: (Ord a) => [a] -> [a] -> [a]
        merge = (++)
    ```
- Normal forms (NF), weak head normal forms (WHNF), reducible expressions (redex), and thunks?
  - [apfelmus](https://apfelmus.nfshost.com/articles/lazy-eval-intro.html)
  - [`foldr`, `foldl`, and `foldl'`](https://wiki.haskell.org/Foldr_Foldl_Foldl%27)
    - IMHO...
      - thunks store redexes...?
      - naive `foldr` simply blows the stack for it's not tail call and we want to start from end of list
        - s.t. infinite lists or long (but finite) lists quickly leads stack overflow
      - naive `foldl` at first glance behaves better for it allocates a thunk storing the redex (according to the function supplied) then tail calls `foldl`... or that's what I assumes
        - s.t. infinite lists would eat all your RAM without stack overflow
        - when the recursion hits the bottom, we now have to actually evaluate the thunks we just allocated, which unfortunately is most likely not tail call: say `(+)` is the function, the recursion bottom, i.e. the actual value we should resolve to, is last of list plus second last thunk, second last thunk is second last of list plus third last thunk, so on and so forth: each thunk is not fully determined without its previous thunk _and_ some additional calculation, which prevents tail call.
          - s.t. long (but finite) lists would run for a while before again stack overflow
- [undefined](https://stackoverflow.com/a/16750944/9933842)

## Ch4 Exercises

### 4.8.1 Split A List From The Middle

Note that Haskell `:`/`!!` are _linked lists_ in its barebones form: both [`length`](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Data-List.html#v:length) and [`!!`](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Prelude.html#v:-33--33-) takes _linear time_!

Meaning without resorting to `splitAt` prelude library function, blunt implementation like this for 4.8.1 results in abyssmal $O(n^2)$ time complexity:

```haskell
-- Quadratic time implementation of 4.8.1 due to linked list nature
halve :: [a] -> ([a], [a])
halve xs =
  if length xs `mod` 2 == 1
    then
      undefined
    else do
      let half_len = length xs `div` 2
      unzip $ map (\i -> (xs !! i, xs !! (half_len + i))) $ take half_len [0 ..]
```

This below seemed better, but due to singly linked list nature, `++` takes linear time on LHS! So still a bummer. OTOH note `(:) :: a -> [a] -> [a]` prepends at front and is generally fine.

```haskell
-- Quadratic time implementation of 4.8.1 due to linked list nature
halve :: [a] -> ([a], [a])
halve xs = halve' [] xs xs
  where
    halve' :: [a] -> [a] -> [a] -> ([a], [a])
    halve' _ _ [_] = undefined
    halve' prefix suffix (_ : _ : zs) = halve' (prefix ++ take 1 suffix) (drop 1 suffix) zs -- recursive (++) makes running time quadratic!
    halve' prefix suffix [] = (prefix, suffix)
```

Speaking of which, linear time constant space singly linked list reversal in imperative programming languages such as C is trivial once you know pointers: for each node `n`, assuming we know its next node is non-nil `m`, then backup `m`'s pointer to next node (which takes constant space) and modify that pointer to point to `n`, rinse and repeat.

The answer is to use call stack as storage: prepend the result returned from recursion. Turns out [prelude `take`](https://hackage-content.haskell.org/package/ghc-internal-9.1401.0/docs/src/GHC.Internal.List.html#take) uses similar technique. In particular this sort of _pair of slow and fast pointers_ technique is also known as _tortoise-and-hare_. Unfortunately this is probably linear space for it's not tail call... or could we do better?

```haskell
-- 4.8.1 split list at the middle
halve :: [a] -> ([a], [a])
halve xs = halve' xs xs
  where
    halve' [] _ = ([], []) -- this is just to match all patterns for we expect the latter argument to be longer
    halve' _ [_] = undefined -- unhappy base case: bail out if length is odd
    halve' suffix [] = ([], suffix) -- happy base case: recursion bottomed out
    -- While there's two more, leave one on the call stack and recurse deeper,
    -- so when bottom out, we have left exactly half of them on the stack, leaving us the required suffix.
    -- On our way returning to the caller, we can stitch together those left on the call stack to get prefix.
    halve' (p : suffix2b) (_ : _ : tail') = (\(prefix, suffix) -> (p : prefix, suffix)) $ halve' suffix2b tail'
```

# Chapter 5 List Comprehension

> The symbol `|` is read as _such that_, `<-` is read as is _drawn from_, and the expression `x <- [1..5]` is called a _**generator**_. A _**list comprehension**_ can have more than one generator, with successive generators being separated by _commas_.

> ...later generators as being more deeply nested, and hence changing the values of their variables more frequently than earlier generators. Later generators can also depend upon the values of variables from earlier generators.

```haskell
zs = [(x, y) | x <- [1..3], y <- [x..3]]
-- [(1, 1), (1, 2), (1, 3), (2, 2), (2, 3), (3, 3)]
concat :: [[a]] -> [a]
concat xss = [x | xs <- xss, x <- xs]
firsts :: [(a, b)] -> [a]
firsts ps = [x | (x, _) <- ps]
length :: [a] -> Int
length xs = sum [1 | _ <- xs]
```

> _guards_ to filter the values produced by earlier generator

```haskell
factors :: Int -> [Int]
factors n = [x | x <- [1..n], n `mod` x == 0]
prime :: Int -> Bool -- N.B. for composites, `factors` short-circuits thanks to lazy evaluation
prime n = factors n == [1, n]
-- this is clearly inefficient...
-- turns out sieve of Eratosthenes has an elegant Haskell implementation thanks to laziness
primes :: Int -> [Int]
primes n = [x | x <- [2..n], prime x]
```

## Ch5 Questions

- [`Foldable`](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Prelude.html#t:Foldable) requires only `foldr` and `foldMap`: apparently you can actually make `foldl` out of them?
- [`foldMap`](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Prelude.html#v:foldMap) is ad-hoc polymorphic over type class [`Monoid`](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Prelude.html#t:Monoid). Wut?

# Chapter 6 Recursion

```haskell
-- insertion sort
isort :: (Ord a) => [a] -> [a]
isort = foldr insertIntoSorted []

-- Inserts element into sorted array (assuming non-decreasing)
insertIntoSorted :: (Ord a) => a -> [a] -> [a]
insertIntoSorted x [] = [x]
insertIntoSorted x ys@(y : zs)
    | y < x = y : insertIntoSorted x zs
    | otherwise = x : ys
```

Note in particular your [^lsp] may hint you that the naive implementation of insertion sort may be refactored with `foldr`: see the left-associative pattern here?

```haskell
isort :: (Ord a) => [a] -> [a]
isort [] = []
isort (x : xs) = insertIntoSorted x (isort xs)
```

[^lsp]: haskell-language-server version: 2.13.0.0 (GHC: 9.10.3)

_Multiple recursion_ refers to the recursion happens more than once, e.g. quick sort and (naive) Fibonacci implementation

```haskell
stableQsort :: (Ord a) => [a] -> [a]
stableQsort [] = []
stableQsort (pivot : xs) =
  let smallers = [s | s <- xs, s < pivot]
      largers = [l | l <- xs, l < pivot]
      equals = [e | e <- xs, e == pivot]
   in stableQsort smallers ++ [pivot] ++ equals ++ stableQsort largers

fib :: (Integral a) => a -> a
fib n
  | n < 0 = undefined
  | n == 0 = 1
  | n == 1 = 1
  | otherwise = fib (n - 1) + fib (n - 2)
```

## `foldl` and `foldr`, revisited with `scanl` and `scanr`

Note that we got a list and via left/right associative operations with `foldl`/`foldr` we get a single result. At times we're also interested in how we actually got that final result. All is not lost: we got `scanl` and `scanr`. Again, they are about recursively applying left/right associative functions.

Again, think of magnets. For `scanl`, think of $\{ \text{seed}, a_0, a_1, a_2, a_3, \cdots \}$, so one by one we snap 'em together to get $\{ f(\text{seed}, a_0), a_1, a_2, a_3, \cdots \}$ then $\{ f(f(\text{seed}, a_0), a_1), a_2, a_3, \cdots \}$ - remember we're trying to record the process, so this is what we actually get: $\{s, f(s, a_0), f(f(s, a_0), a_1), \cdots \}$.

But wait, that means `scanl` simply gets us the result of applying the _same exact function again and again_ on some `seed :: b`, except also with some extra argument specified by the list `[a]`... so basically a fancier prelude `iterate`! Note also how `scanl` and `iterate` are so close to loop with mutating states like we often do in imperative languages. Anyhow with this in mind the implementation unfolds itself naturally:

```haskell
myScanl :: (b -> a -> b) -> b -> [a] -> [b]
myScanl f seed xs =
  let myIterateWith _ _ [] = []
      myIterateWith g seed' (y : ys) = sprout : myIterateWith g sprout ys
        where
          sprout = f seed' y
   in seed : myIterateWith f seed xs
```

And thus just like `iterate`, `scanl` inherently deals with infinite lists just fine, no matter what the supplied function actually does. For example:

```haskell
print $ take 67 $ scanl (+) 69 [42..]
```

One might ponder, hold a sec, then why does `foldl` behave worse in the sense no matter what function is supplied, as long as the input list is infinite, both `foldl` and `foldl'` just hangs? Since `scanl` does _more_ things than `foldl`...?

Well for `foldl` on infinite lists, the "expression" (I dunno if this is the correct terminology) we need to "resolve" is _always_ a `foldl`. _Always_. That your function is not strict or lazy with certain patterns does not matter: we need to know what the `foldl` means, then we need to unpack it only to get yet another `foldl` thunk, and thus in runtime it does nothing but allocating thunks on the heap like crazy. In `scanl` terms, it's like asking the _last_ element of the infinite list, and thus it hangs just like why `(last . repeat) ()` also hangs.

I mean consider the magnet analogy again: $\{s, a_0, a_1, a_2, a_3, \cdots\}$ and one by one snap 'em with $f$: $\{s, f(s, a_0), f(f(s, a_0), a_1), f(f(f(s, a_0), a_1), a_2), \cdots \}$: as long as we ask only finite amount of them, we need only linear time overhead to form such list (I mean you could Hanoi tower with `scanl` and that's definitely not linear time but that's $f$'s fault).

Now to `scanr`. Using the magnet analogy again, what we want to do is record the process of iteratively snapping 'em, i.e. $\{ a_0, a_1, a_2, \cdots, a_n, \text{seed} \}$, $\{ a_0, a_1, a_2, \cdots, a_{n-1}, f(a_n, \text{seed}) \}$, $\{ a_0, a_1, a_2, \cdots, a_{n-2}, f(a_{n-1}, f(a_n, \text{seed})) \}$ and get the list

$$
\begin{aligned}
\{ \\
  & f(a_0, f(a_1, f(a_2, \cdots, f(a_n, \text{seed})))), \\
  & f(a_1, f(a_2, f(a_3, \cdots, f(a_n, \text{seed})))), \\
  & \cdots, \\
  & f(a_{n-1}, f(a_n, \text{seed})), \\
  & f(a_n, \text{seed}), \\
  & \text{seed} \\
\}
\end{aligned}
$$

As `scanr` is just `foldr` except now we don't let the information just pass by, `scanr` performs similarly to `foldr` when given infinite list: it _exposes the function patterns in each step_ and thus as long as the function is not _strict_ (?) in the second argument and we do not ask for infinite far behaviors we're good to go just like `foldr`, and in which case it's ok to simply use `undefined` as the seed for it does not matter.

```haskell
-- ok
take 5 $ scanr (\n -> const (n `rem` 4 == 0)) undefined [1,4..]
-- loops
last $ scanr (\n -> const (n `rem` 4 == 0)) undefined [1,4..]

-- ok: `(head . scanr) == foldr`
take 15 $ scanr (\m -> const (if m >= 7 then 7 else m)) undefined [-2,-1..] -- `[-2,-1,0,1,2,3,4,5,6,7,7,7,7,7,7]`
foldr (\m -> const (if m >= 7 then 7 else m)) undefined [-2,-1..] -- `-2`
```

```haskell
myScanr :: (a -> b -> b) -> b -> [a] -> [b]
myScanr _ seed [] = [seed]
myScanr f seed (x : xs) = f x y : ys
  where
    ys = myScanr f seed xs
    y = head ys
```

So in terms of friendliness with regard to infinite input, one might say `scanl` takes the lead since it simply gives another input list, followed by `foldr` and `scanr` of which behavior depends heavily on the input function, followed by `foldl`/`foldl'` which never gives any value and became a memory hog.

It's all about associativity and how the "magnets" are placed!

## Ch6 Questions

- `and`/`or`/`any`/`all` are implemented [via](https://hackage-content.haskell.org/package/base-4.22.0.0/docs/Prelude.html#g:14) `foldMap`. Why not simply `foldr`?
  - ```haskell
    all' :: (a -> Bool) -> [a] -> Bool
    all' f xs = foldr (\x b -> f x && b) True xs -- ordering matters for laziness and ability to take infinite lists
    any' :: (a -> Bool) -> [a] -> Bool
    any' f xs = foldr (\x b -> f x || b) False xs -- ordering matters for laziness and ability to take infinite lists
    ```
- How to know if a function is lazy/strict in certain arguments? Is this DIY `(++)` lazy in any of its two arguments?
  - ```haskell
    app :: [a] -> [a] -> [a]
    [] `app` ys = ys
    (x:xs) `app` ys = x : (xs `app` ys)
    ```
  - `drop 1 [1,3,5,undefined]` throws exception: is `drop` somewhat lazy?
- Fibonacci sequences, in forward order?

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
