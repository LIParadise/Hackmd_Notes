---
tags: programming, CS, notes, algorithm, Rust, type system, variance, invariant, covariant, contravariant
---

In Rust, the lifetime is more of a type system concept, which is ever so slightly more restrict than what's strictly required for soundness: most of time the type check fails because of the semantics ain't right, rather than the type system being too restrictive s.t. valid code are rejected.

Anyway the type system is built around [variance](https://doc.rust-lang.org/nomicon/subtyping.html): Rust is strongly typed, including but not limited to the fact the lifetime type system needs to match exactly. Naive implementation quickly become way more restrictive than than that required by soundness, so that's where _variance_ kicks in: it specifies when some type may coerce into some other type in lifetime related context, s.t. we may calculate at function call site whether the call is valid i.e. if the types match.

Let's first accept that if some lifetime `'a` is superset of some other lifetime `'b`, we say that `'a` is _subtype_ of `'b`. You see, type systems have some resembalance with generic logical sets, and subtypes are like subsets, meaning they fall complete within some superset/supertype. Just like subsets are, well, subsets of the containing superset, subtypes may coerce into the corresponding super type.

But a standalone lifetime parameter `'a` is nothing more than a generic type parameter: they are not immediately the types to which the Rust variables belong. Rust has something to say about the generic types around various generic type parameters.

## Covariant

Given some generic type `F` with its generic type parameter `T`, i.e. `F<T>` (e.g. `&'a T`: the `F` here is the `&`, with two type parameters `'a` for lifetime and `T` for the pointee), we say that `F` is _covariant_ over `T` if the subtype relation of `T` carries over to `F<T>`, i.e. if `U` is subtype of `T`, then `F<U>` is subtype of `F<T>`.

Being _covariant_ is probably the more straightforward sort of relation between generic types. A `Box<T>` is covariant over `T`: when some function requires a `Box<&'a str>`, there's nothing wrong throwing a `Box<'static str>` at it, similarly passing a `&'static str` where `&'a str` is required is fine.

```rust
fn longer<'a>(s: &'a str, bs: Box<&'a str>) -> &'a str {
    match s.len().cmp(&bs.len()) {
        std::cmp::Ordering::Greater => s,
        std::cmp::Ordering::Less => &*bs,
        std::cmp::Ordering::Equal => "???",
    }
}

fn foo(s: &str) {
    println!("{}", longer(s, Box::new("foo")));
}

fn main() {
    foo("bar");
}
```

## Contravariant

It might be slightly daunting at first glance, but let's see: `Fn(T) -> U` is covariant over `U` but _contravariant_ over `T`. Why? When a function requires `&'static str`, it's probably since it internally requires the referent of its parameter to be available for indefinitely long, s.t. it may safely cache it somewhere as a side effect.

```rust
use std::sync::Mutex;

static S: Mutex<Option<&str>> = Mutex::new(None);

fn foo<'a>(strings: &[&'a str], f: impl Fn(&'a str)) {
    strings.iter().for_each(|s| f(s));
}

fn bar(s: &str) {
    println!("{s}");
}

fn bad(s: &'static str) {
    println!("{s}");
    *S.lock().unwrap() = Some(s);
}

fn main() {
    foo(&["hello", &String::from("world")], bar);
    foo(&["hello", &String::from("world")], bad); // does not compile
}
```

Similarly, when somebody requires a `impl Fn(&'static str)`, there's nothing wrong we throw a `impl Fn(&'a str)` at it: when one specifies it needs `impl Fn(&'static str)`, it means it in turn passes only `&'static str` into the function, and we've already know that coercing a `&'static str` into a `&'a str` (in order to call our function) is fine.

## Invariant

_Invariant_ refers to the relation when no subtype relation of `T` is carried over to `F<T>`, and every generic parameter of `F` must match exactly: this snippet fails to compile since `&'a mut T` is invariant over `T`.

```rust
#[derive(Debug)]
struct Foo<'f>(#[allow(unused)] &'f str);

fn foobar<'f>(temp: &mut Foo<'f>, static_: &mut Foo<'static>) {
    std::mem::swap(temp, static_);
}
```

The intuition is something like this: say we have a `Foo<'f>`, meaning it borrows from some transient value only valid for some duration in some specific execution of the program, OTOH `Foo<'static>` is borrowing nothing from any specific run of program executions, s.t. the referents shall not be exchanged places.

As a side note, this compiles just fine:

```rust
#[derive(Debug)]
struct Foo<'f>(#[allow(unused)] &'f str);

fn foo<'f>(f: &mut Foo<'f>) {
    let mut g = Foo("static");

    std::mem::swap(f, &mut g);
    std::mem::swap::<Foo<'f>>(f, &mut g);
}

fn main() {
    let main = String::from("main");
    let mut main = Foo(&main);
    foo(&mut main);
    dbg!(main);
}
```

The reason here is the `g` is actually coerced right at its definition site: it's `Foo<'f>` rather than `Foo<'static>`, since why not: `Foo<'f>` is covariant over `'f`. Local variables have more information for Rust to do various tricks.

As for the [not part](https://users.rust-lang.org/t/variance-not-being-the-whole-story/129096) in the why not, here's another code snippet demonstrating this: both group A and group B should not be enabled. The fact `&'a mut T` being invariant over `T` makes it invalid whether Rust chooses to coerce `g` into `Foo<'f>` right at definition site.

```rust
#[derive(Debug)]
struct Foo<'f>(#[allow(unused)] &'f str);

impl Foo<'static> {
    fn is_static_shared(&self) {}
    fn is_static_exclusive(&mut self) {}
}

fn bar<'f>(#[allow(unused)] f: &mut Foo<'f>) {
    // group A and group B cannot coexist!
    
    let mut g = Foo("static");
    
    {
        // group A
        // g.is_static_shared();
        // (&mut g).is_static_shared();
        // (&mut g).is_static_exclusive();
    }

    {
        // group B
        std::mem::swap(f, &mut g);
        std::mem::swap::<Foo<'f>>(f, &mut g);
    }
}
```

### Invariant Corollaries

```rust
//! https://quinedot.github.io/rust-learning/pf-shared-nested.html

use std::cell::Cell;

fn valid_snek() {
    #[derive(Debug)]
    struct ShareableSnek<'a> {
        owned: &'a str,
        borrowed: Cell<&'a str>,
    }

    #[cfg(true)]
    impl<'a> Drop for ShareableSnek<'a> {
        fn drop(&mut self) {}
    }

    impl<'a> ShareableSnek<'a> {
        fn bite(&self) {
            // it's fine to have non-trivial `impl Drop`:
            // https://quinedot.github.io/rust-learning/pf-shared-nested.html#this-is-still-a-yellow-flag
            // we're merely giving out what we've borrowed by _not_ restricting lifetime on `&self`:
            // it's probably something _shorter_ than `'a`:
            // given `&'short &'long str`, we may have `&'long str` just fine: just copy it out!
            // https://quinedot.github.io/rust-learning/st-invariance.html
            self.borrowed.set(&self.owned);
        }
    }

    let snek = ShareableSnek {
        owned: "🐍",
        borrowed: Cell::new(""),
    };

    snek.bite();

    // Unlike the `&mut` case, we can still use `snek`!  It's borrowed forever,
    // but it's "only" *shared*-borrowed forever.
    println!("{snek:?}");
}

fn invalid_snek() {
    #[derive(Debug)]
    struct ShareableSnek<'a> {
        owned: String,
        borrowed: Cell<&'a str>,
    }

    #[cfg(false)]
    impl<'a> Drop for ShareableSnek<'a> {
        fn drop(&mut self) {
            // this won't compile if enabled
        }
    }

    impl<'a> ShareableSnek<'a> {
        fn bite(&'a self) {
            // since the field being `String` rather than a `&'a str`,
            // we have to use `&'a self`,
            // but the `Self` type is invariant over `'a` thx to the `Cell`,
            // meaning we have no choice but to borrow ourself forever (in a shareable way):
            // the `&'a self` is actually `&'a ShareableSnek<'a>`,
            // and again `ShareableSnek<'a>` is invariant over `'a`!
            self.borrowed.set(&self.owned);
        }
    }

    let snek = ShareableSnek {
        owned: "🐍".to_string(),
        borrowed: Cell::new(""),
    };

    snek.bite();

    // Unlike the `&mut` case, we can still use `snek`!  It's borrowed forever,
    // but it's "only" *shared*-borrowed forever.
    println!("{snek:?}");
}

fn main() {
    valid_snek();
    invalid_snek();
}
```