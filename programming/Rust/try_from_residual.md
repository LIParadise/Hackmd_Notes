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

# Third-Party Crates on Error Handling

Some third-party _error types_ like [`anyhow::Error`](https://docs.rs/anyhow/1.0.102/anyhow/struct.Error.html) or [`eyre::Report`](https://docs.rs/eyre/0.6.12/eyre/struct.Report.html) are made to be supercharged version of `std::error::Error` with backtrace, description, etc. With [their](https://docs.rs/anyhow/1.0.102/anyhow/index.html#details) [corresponding](https://docs.rs/eyre/0.6.12/eyre/#details) `Result` implementation, one may and is supposed to write functions returning such `Result`, in which they may handle boilerplate `Result`-returning subroutines with the `?` operator.

Note that these error types are themselves _not_ `std::error::Error`, but do implement something like `From<E: std::error::Error>`. Due the default blanket `impl From<T> for T` implementation, this is understandable. But why do we _need_ the error types to be `From<E: std::error:Error>` in the first place anyway?

IMHO, _and that's a huge **my humble opinion**_ so take with a huge chunk of salt, this is ultimately for people to play with the `?` operator nicely. Rust had [historically](https://github.com/rust-lang/rfcs/blob/master/text/1859-try-trait.md#desugaring-and-the-try-trait) been sneaking in a `From::from` in the early-return route when dealing with `Result`/`?` desugaring. Thus in order for these crates to make any sense, instead of writing `Result::map_err(MyFancyError::from_std)` or alike everywhere, they took advantage of such desugaring and implements `From<E: std::error::Error>` for their own error types. As such, as long as your customized errors are `std::error::Error`, including such crates becomes a breeze.

This however is indeed a bit counter-intuitive. I mean your error type is supposed to be, well, an error type, so naturally we'd expect them to be `std::error::Error` as well. Again, this unfortunately contradicts with the default blanket implementation.

[Turns](https://gist.github.com/yaahc/9fe7a7631ac35df227e2061bd5d56be3) [out](https://rust-lang.zulipchat.com/#narrow/channel/257204-project-error-handling/topic/separating.20From.3CE.3A.20Error.3E.20from.20Box.3Cdyn.20Error.3E/near/228027529) [this dilemma](https://github.com/rust-lang/rfcs/blob/master/text/3058-try-trait-v2.md#why-does-fromresidual-take-a-generic-type) is solved by `try-trait-v2`: it's the `Try` type that determines with what `Try::Residual` type control flow would be short-circuited, and it's up to the outer type which implements the _generic_ `FromResidual` trait to decide if that specific `Try::Residual` is accepted in what way. In other words, the error types may well be `std::error::Error`, too: it's the `Result` type that needs to have ergonomics built-in, and the way it's implemented is up to the exact `FromResidual` implementation.

# Questions

## <a id="multiple-from-residual-impl"></a> Multiple [`FromResidual`][from-residual-trait] Implementations for Certain Types

At first glance, since every (generic) type may only implement `Try` once, and that exact implementation defines exactly the (generic) associated type `<T as Try>::Residual`, based on which `FromResidual`/`?` is implemented, why some [types](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3COption%3CInfallible%3E%3E-for-Option%3CT%3E) [have](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3CYeet%3C()%3E%3E-for-Option%3CT%3E) [multiple](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3CResult%3CInfallible,+E%3E%3E-for-Result%3CT,+F%3E) [implementations](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#impl-FromResidual%3CYeet%3CE%3E%3E-for-Result%3CT,+F%3E) of [`FromResidual`](https://doc.rust-lang.org/1.95.0/std/ops/trait.FromResidual.html#implementors), for you cannot implement `Try` twice anyway, no? Trait specialization comes to play here, or is `FromResidual` "overloaded" in purpose here? What is [`Yeet`][yeet] anyway?

## When Does The `?` Operator Apply?

I mean even with `#![feature(try_trait_v2, try_trait_v2_residual)]`, it seems one cannot use `?` within arbitrary function, even though that function returns one's customized type which is/implements `Try`...?

The gist is that while currently (again, May 2026, Rust 1.95.0/1.98.0) only within functions that return `Option`/`Result` one may invoke the `?` operator, the exact type of the expression upon which `?` acts is sort of loose. We may say `let _ = T::new(/* omit */)?` inside function that returns `Ret` as long as...

- `T: Try`
- `Ret: FromResidual<<T as Try>::Residual>`

So like in the [demo](#the-question-mark-operator-), `MyTry: Try` and `Result: FromResidual<BareResidual>`, and thus we may say `_ = MyTry(false)?;` in a function that returns `Result`.

To reiterate, it's the type upon which we apply the `?` operator that needs to be `Try`, provided its `Try::Residual` is _convertable_ to the return type, in the sense that the returning type has `FromResidual` implementation for that `Try::Residual`.

So the idea has been the same (again, May 2026, Rust 1.95.0/1.98.0): only within functions that gives `Option`/`Result` we may use the `?` operator; the `Try` gives us freedom as to upon what additional types we may call the `?` operator, and to facilitate this functionality we need to implement `FromResidual` for the _foreign_ types `Option`/`Result`.

This also explains why certain types have [_multiple_](#multiple-from-residual-impl) `FromResidual` implementations. This doesn't exactly make sense at first glance, since you cannot `impl Try` for them multiple times anyway; it's to make functions that return them have more freedom, in the sense that within those functions we have more types at which we may throw a `?` operator.

Speaking of which, now that I think about it, it's kinda intriguing we may `impl<T, E: Default> FromResidual<BareResidual> for Result<T, E>` here. Huh. Orphan rules...?

## What Is [`try_trait_v2_residual`][try-trait-v2-residual] Anyway?

If you're gonna play with `Try` in May 2026, you're gonna need both `try_trait_v2` and `try_trait_v2_residual` enabled using nightly (`cargo 1.98.0-nightly (4d1f98451 2026-05-15)`). Basically boils down to implement `std::ops::Residual` for your (generic) associated type `<T as Try>::Residual` and _link_ to the `T: Try`.

## So-Called Try Blocks: `try {}`

What are they? And what are _homogeneous try blocks_? Meaning there's possibility for _heterogeneous try blocks_...?

# [`Poll<Result<T, E>>`][poll-result]

```rust
impl<T, E> Try for Poll<Result<T, E>> {
    type Residual = <Result<T, E> as Try>::Residual; // Result<Infallible, E>
    type Output = Poll<T>;
}
impl<T, E> FromResidual<Result<Infallible, E>> for Poll<Result<T, E>> {}
```

Again, implementing `Try` makes you applicable to the `?` operator in certain functions, and your `Try::Residual` determines exactly what functions (specifically what type does the function return) you are actually allowed to do this.

Since `<Poll<Result<T, E>> as Try>::Residual` is specified exactly same as `<Result<T, E> as Try>::Residual` i.e. `Result<Infallible, E>`, we have four cases:

```rust
fn make_pr<T, E>() -> Poll<Result<T, E>> {}
fn pr_in_pr<T, E>() -> Result<T, E> { _ = make_pr()?; }
fn pr_in_r<T, E>() -> Result<T, E> { _ = make_pr()?; }
fn r_in_pr<T, E>() -> Result<T, E> {}
fn r_in_r<T, E>() -> Result<T, E> { /* generic `Result` type, not interesting. */}
```

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

## Another Demo

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

## The Question Mark Operator `?`

```rust
#![feature(try_trait_v2, try_trait_v2_residual)]

use std::{
    fmt::Debug,
    ops::{ControlFlow, FromResidual, Residual, Try},
};

struct MyTry(bool);
impl Try for MyTry {
    type Residual = BareResidual;
    type Output = ();
    fn from_output(_: <Self as Try>::Output) -> Self {
        MyTry(true)
    }
    fn branch(self) -> ControlFlow<<Self as Try>::Residual, <Self as Try>::Output> {
        match self.0 {
            true => ControlFlow::Continue(()),
            false => ControlFlow::Break(BareResidual),
        }
    }
}
impl FromResidual<BareResidual> for MyTry {
    fn from_residual(_: BareResidual) -> Self {
        MyTry(false)
    }
}
impl Residual<()> for BareResidual {
    type TryType = MyTry;
}

struct BareResidual;
impl<T, E> FromResidual<BareResidual> for Result<T, E>
where
    E: Default,
{
    fn from_residual(_: BareResidual) -> Self {
        Err(E::default())
    }
}

fn test_result<O: Debug, E: Default + Debug>(ret: Result<O, E>) -> Result<(), E> {
    _ = MyTry(ret.is_ok())?;
    println!("{ret:?} is uninteresting...");
    ret.map(|_| ())
}

fn main() {
    test_result(Ok::<i32, u32>(114514)).unwrap();
    _ = test_result(Err::<i32, u32>(67)).inspect_err(|e| println!("Now we're talking: {e:?}"));
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
[poll-result]: https://doc.rust-lang.org/1.95.0/std/task/enum.Poll.html#impl-Try-for-Poll<Result<T,+E>>
[result-as-try]: https://doc.rust-lang.org/1.95.0/std/ops/trait.Try.html#associatedtype.Residual-3
[result-as-from-residual]: https://doc.rust-lang.org/1.95.0/std/result/enum.Result.html#impl-FromResidual%3CResult%3CInfallible,+E%3E%3E-for-Result%3CT,+F%3E
[yeet]: https://doc.rust-lang.org/1.95.0/std/ops/struct.Yeet.html
