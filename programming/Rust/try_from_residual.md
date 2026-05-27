# [`Try`][try-trait]

For a generic type to be/implement `Try`, it's of utter importance to know about the two (generic) associated types: `Try::Output` and `Try::Residual`.

In a nutshell, the `Try::Output` is the type you want in the _normal_ control flow: the `T` in `Option<T>`/`Option::Some`, `Result<T, E>`/`Result::Ok`, and `ControlFlow<Break, T>`/`ControlFlow::Continue`.

OTOH, the `Try::Residual` is the type that you're gonna _throw back to the caller via the `?` operator_, your _early return payload_, which typically is a type that **looks like but not directly the same** as aforementioned type that used in early return.

WTF?

Let's first note that [`FromResidual<R = <Self as Try>::Residual>`][from-residual-trait] is a _super_ trait of `Try`: if you have not noticed, this is a super trait that is "generic" over the (generic) associated type of the subtrait!

So it boils down to how we imagined `?` operator to work. In a function that returns `Option<T>`, we _probably_ would like a shorthand for when a sub-routine that returns `Option<U>` wants to do early return in a straightforward way. That is, the early return control flow is gonna require something that both describes and _hides_ the straightforwardness of transforming an `Option::<T>::None` into an `Option::<U>::None`.

Thus `<Option<T> as Try>::Residual` is [simply a concrete type `Option<Infallible>`][option-as-try] to capture the _`Option::None`-ness_: in some sense every `Option::None` is the _same_ as `Option::<Infallible>::None`: there's _nothing_, why bother thinking about "what if there's something"?

In the same vein, suppose when we're writing a function that returns `Result<Okay, Ominous>` and we're calling some sub-routine that returns `Result<Ideal, Inferior>`. Maybe your `Ominous` is sort of kitchen sink catch-all: the point is you _know_ you simply want to throw that nasty `Inferior` back to the caller, and you know it's doable. You know that `Ominous: From<Inferior>`. You know it's _perfectly fine_ that `Okay` and `Ideal` are _completely irrelevant_ types. So what you really want, under this convention, is that `<Result<T, E> as Try>::Residual` should better be some _GAT_ or _generic associated type_, most likely generic over `E`, that captures the idea that the the outer `Ominous` is obtainable via an `Inferior` instance.

Thus the _GAT_ for `Result`: `<Result<T, E> as Try>::Residual` is defined to be `Result<Infallible, E>`, in order to capture the idea that there _is_ some value of certain type `E` (that we want to transform) and nobody care about `T` for now, and thus the `FromResidual` trait for `Result<T, E>` is now officially _generic_ (and thus requires _monomorphization_): we want to transform to some different error type, here `Inferior` to `Ominous`, so it falls out to be [`impl<Okay, Inferior, Ominous: From<Inferior>> FromResidual<Result<Infallible, Inferior>> for Result<Okay, Ominous>`][result-as-from-residual].

Note that this design has a merit that when inside a function returning `Option<T>`, one can *never* accidentally trigger early return via `?` over types other than `Option`, e.g. `Result::<T, E>::Err(e)?`, for the only `FromResidual<X>` which `Option<T>` implements is `X = Option<Infallible>` as defined in `<Option as Try>::Residual`. In other words, even though they all uses `?` operator, we maintain (up to the implementors) certain sort of distinction: explicit over implicit.

# Questions

## Multiple [`FromResidual`][from-residual-trait] Implementations for Certain Types

At first glance, since every (generic) type may only implement `Try` once, and that exact implementation defines exactly the (generic) associated type `<T as Try>::Residual`, based on which `FromResidual`/`?` is implemented, why some [types](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3COption%3CInfallible%3E%3E-for-Option%3CT%3E) [have](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3CYeet%3C()%3E%3E-for-Option%3CT%3E) [multiple](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3CResult%3CInfallible,+E%3E%3E-for-Result%3CT,+F%3E) [implementations](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3CYeet%3CE%3E%3E-for-Result%3CT,+F%3E) of [`FromResidual`](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#implementors), for you cannot implement `Try` twice anyway, no? Trait specialization comes to play here, or is `FromResidual` "overloaded" in purpose here? What is [`Yeet`][yeet] anyway?

## When Does The `?` Operator Apply?

I mean even with `#![feature(try_trait_v2, try_trait_v2_residual)]`, it seems one cannot use `?` within arbitrary function, even though that function returns one's customized type which is/implements `Try`...?

## What Is [`try_trait_v2_residual`][try-trait-v2-residual] Anyway?

If you're gonna play with `Try` in May 2026, you're gonna need both `try_trait_v2` and `try_trait_v2_residual` enabled using nightly (`cargo 1.98.0-nightly (4d1f98451 2026-05-15)`). Basically boils down to implement `std::ops::Residual` for your (generic) associated type `<T as Try>::Residual` and _link_ to the `T: Try`.

# Demos

## Super Trait

This one works with 1.95.0 stable (`cargo 1.95.0 (f2d3ce0bd 2026-03-21)`).

```rust
use std::{
    convert::Infallible,
    io,
    task::{Poll, ready},
};

fn wut(b: bool) -> Poll<io::Result<()>> {
    if b {
        ready!(Poll::Ready(io::Result::Ok(())))?
    } else {
        ready!(Poll::Ready(io::Result::Err(io::Error::from(
            io::ErrorKind::TimedOut
        ))))?
    }
    Poll::Ready(io::Result::Ok(()))
}

fn hehe() -> Option<&'static str> {
    _ = Result::<u32, ()>::Err(())?;
    unreachable!();
}

/// [try_trait_v2](https://github.com/rust-lang/rust/issues/84277)
/// [`FromResidual`](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html)
trait Try: FromResidual {
    type Residual;
}
trait FromResidual<T = <Self as Try>::Residual> {
    fn from_residual(_: T) -> Self;
}

impl<T, R, E: From<R>> FromResidual<Result<Infallible, R>> for Result<T, E> {
    fn from_residual(result: Result<Infallible, R>) -> Self {
        Err(result.map_err(E::from).unwrap_err())
    }
}
impl<T, E: Default> FromResidual<Option<Infallible>> for Result<T, E> {
    fn from_residual(_: Option<Infallible>) -> Self {
        Err(E::default())
    }
}
impl<T, E> Try for Result<T, E> {
    type Residual = Result<Infallible, E>;
}

fn main() {
    println!("{:?}", wut(true));
    println!("{:?}", wut(false));
}
```

# Another Demo

This plays with `Try`/`FromResidual`/`Residual`: it's possible to make your type accomodate `<T as Try>::Residual` for several `T`: another trait!

Assumes `cargo 1.98.0-nightly (4d1f98451 2026-05-15)`.

```rust
#![feature(try_trait_v2, try_trait_v2_residual)]
#![allow(unused)]

use std::{
    any::type_name_of_val,
    convert::Infallible,
    ops::Try,
    ops::{ControlFlow, FromResidual},
};

enum Triable<O, R> {
    Output(O),
    Residual(R),
}
trait RecyclableResidue {}

impl<O, R> Try for Triable<O, R>
where
    R: RecyclableResidue,
{
    type Residual = Triable<Infallible, R>;
    type Output = O;
    fn branch(self) -> ControlFlow<<Self as Try>::Residual, <Self as Try>::Output> {
        match self {
            Triable::Residual(r) => ControlFlow::Break(Triable::Residual(r)),
            Triable::Output(o) => ControlFlow::Continue(o),
        }
    }
    fn from_output(output: <Self as Try>::Output) -> Self {
        Triable::Output(output)
    }
}
impl<O, R: RecyclableResidue> std::ops::Residual<O> for Triable<Infallible, R> {
    type TryType = Triable<O, R>;
}
impl<O, R: RecyclableResidue> FromResidual<Triable<Infallible, R>> for Triable<O, R> {
    fn from_residual(residual: Triable<Infallible, R>) -> Self {
        let Triable::Residual(residual) = residual;
        Triable::Residual(residual)
    }
}

impl<O, R> RecyclableResidue for ControlFlow<R, O> {}
impl<O, R> RecyclableResidue for Result<O, R> {}

fn main() {
    let v = vec![
        Triable::Output(()),
        Triable::Residual(Result::<Infallible, _>::Err("野獣先輩")),
    ];
    v.into_iter().try_for_each(std::convert::identity);

    let v = vec![
        Triable::Output(()),
        Triable::Residual(ControlFlow::<_, Infallible>::Break("野獣先輩")),
    ];
    v.into_iter().try_for_each(std::convert::identity);
}
```

# References

[`std::ops::Try`][try-trait]
[`std::ops::Infallible`][from-residual-trait]
[try-trait-v2 RFC][try-trait-v2-rfc]
[`std::convert::Infallible`][infallible]
[GitHub Discussion][github-rfc-discussion]

[infallible]: https://doc.rust-lang.org/1.95.0/std/convert/enum.Infallible.html
[from-residual-trait]: https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html
[try-trait-v2-rfc]: https://github.com/rust-lang/rfcs/blob/master/text/3058-try-trait-v2.md
[try-trait-v2-residual]: https://github.com/rust-lang/rust/issues/91285
[try-trait]: https://doc.rust-lang.org/1.95.0/std/ops/trait.Try.html
[github-rfc-discussion]: https://triagebot.infra.rust-lang.org/gh-comments/rust-lang/rust/issues/84277
[option-as-try]: https://doc.rust-lang.org/1.95.0/std/ops/trait.Try.html#associatedtype.Residual-2
[result-as-try]: https://doc.rust-lang.org/1.95.0/std/ops/trait.Try.html#associatedtype.Residual-3
[result-as-from-residual]: https://doc.rust-lang.org/1.95.0/std/result/enum.Result.html#impl-FromResidual%3CResult%3CInfallible,+E%3E%3E-for-Result%3CT,+F%3E
[yeet]: https://doc.rust-lang.org/1.95.0/std/ops/struct.Yeet.html
