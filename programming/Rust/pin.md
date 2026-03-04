# `Pin`

![pointers are hard](https://i.imgflip.com/alnx3i.jpg)

If you think you know pointers you should better... write some blog post, publish some papers, and maybe contribute back to Rust, since the fact is we need an optimizing compiler and we need to have fast hardware, so after taking aliasing rules and memory ordering [into consideration](https://github.com/rust-lang/unsafe-code-guidelines/issues/585) and everything is non-trival: as of writing the repo has over 200 open issues.

Anyhow I assume you're already familar with `Future` and have used `smol`/`tokio` for a while. Maybe you've heard of _intrusive_ data structures.
Anyway here's some brief overview.

## `Future`

When we have some type `T: Future`, instances of `T` are _(finite) state machines_ ~~monads~~. Say you write any `async` block, that block is an instance of some anonymous type that implements `Future`, again an FSM.
Such constructs are supposed to be [`poll`-ed](https://doc.rust-lang.org/std/future/trait.Future.html#tymethod.poll) by some external runtime, switching its internal states, and depending on its environments - mostly I/O or timer - return a [`Poll`](https://doc.rust-lang.org/std/task/enum.Poll.html), telling the guy (runtime) who `poll`-ed it either that _you'll have to wait_ or that _here's the value_.
If you're writing your customized type that implements `Future`, you most likely need to store the runtime information _somewhere_ when `poll`-ed: the contract of Rust `Future` is that they are _lazy_, meaning just like Linux `poll`/`epoll`, runtime being somewhat like kernel scheduler, most likely they won't ever `poll` you again: after all it's you who told 'em to wait. You'll have to _somehow_ notify the runtime, thereby [`wake`](https://doc.rust-lang.org/std/task/struct.Waker.html#method.wake_by_ref) yourself, s.t. you as a FSM is scheduled to be `poll`-ed again by the runtime.

## Intrusive Data Structure

In Rust, you write your container as `struct MyFancyDataStructure<T>` and now you rock. Not in C, at least before C11 `_Generic`. Or at times you know you have _something_ of which _lifetime_ is logically maintained somehow and you don't want to move it because of allocator overheads.
Enter _intrusive_ data structures.
The trick is actually fairly straightforward: for lists/trees, the whole point are the links between nodes, so we simply embed _pointers_ into your original `struct`. Your simple `struct Foo { uint32_t val; };` now becomes `struct Foo { uint32_t val; struct list_head list; }`, and you may now use kernel [macro](https://github.com/torvalds/linux/blob/master/include/linux/container_of.h) [magic](https://github.com/torvalds/linux/blob/master/include/linux/list.h) to traverse the list.
I.e. you do not do [monomorphization](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html) based on input generic type parameter `T`, instead you hack the internals of `T` to be `U` s.t. `U` has all the necessary pointers to live inside certain lists/trees.
Now that you implemented your data structure, you now realized you're _pinned in place_: your neighbors must be some type that had gone over similar process, in particular they too have pointers pointing to you, so you better _not move_!

## Why We Need `Pin`

The point is, these are constructs that we don't want them to ever move.

For `Future`, it's since you as a Rust programmer definitely want `async` code to _feel_ the same as generic Rust code, in particular you'd want to use _references_.
You want _pointers_ that may (or may not, who knows) point to your _stack_ variables, except when you realize you don't really have a stack: you're but some passive `struct`, you do not own the usual execution context. The whole stack concept, implemented with modifying `sp`, `jalr`, and `ret`, is simply absent: it's the runtime who own the execution context.
You are but a mere finite state machine.
All you can do is make your type _big_ enough s.t. you may place all the transient values in yourself besides your state information.
This is when you realized now you too cannot be moved, just like intrusive data structures, except this time the pointers are those borrows of your "stack" variable.
They are derived from pre-calculated offset associated within you as an FSM. They are your self pointers.

You might wonder, at least for self pointers, why not just compile references in `async` blocks not to pointers in the C sense but simply _offsets_ from the `this` pointer equivalent?
The problem is, [you cannot know statically](https://without.boats/blog/pin) if a reference is a synchronous context one (thus pointers in C sense) or an `async` one. So we are stuck with traditional pointers.

However, note how Rust is designed s.t. moves are everywhere and every type is supposed to be trivially movable, and there's supposed to be no observable effect if compiler did move anything.

After all we're talking about pointers, and pointers are `unsafe` in Rust.
So the objective becomes that we need some mechanism to enforce certain invariants (the item not moved, plus that the `Drop` is always called before the space got repurposed); only then we can write `unsafe` code that are actually _sound_.
This is exactly why we have `!Unpin` and `Pin`. They are compiler enforced invariants for programmers to build a safe and sound API around `unsafe` pointer black magic.

## The Gist of `Pin`

Just like `Send` and `Sync`, turns out `Unpin`/`!Unpin` is also all about your API design.

Say your `struct` have _self pointers_ (intrusive data structures) or simply want to expose some associated function that modify yourself but at the same time secretly store pointers to yourself somewhere, which may be used later to [point back to yourself](https://doc.rust-lang.org/1.93.0/std/pin/index.html#fn2) (custom `Future` [implementations](https://docs.rs/tokio/1.49.0/src/tokio/time/sleep.rs.html#132-137)).
You need `unsafe` to work with instances of your type, in particular you don't want safe code to have any chance moving your values.
So you _must_ mark the [receiver](https://doc.rust-lang.org/1.93.0/std/ops/trait.Receiver.html) as `Pin<&mut Self>` (`Pin<&Self>` works albeit [tricker](https://users.rust-lang.org/t/why-is-it-unsafe-to-pin-a-shared-reference/40309/12)), _and_ you hold a [`PhantomPinned`](https://doc.rust-lang.org/std/marker/struct.PhantomPinned.html) to mark yourself as `!Unpin`.
In associated functions of which receiver is `Pin<Ptr: Deref<Target = T>>`, your `unsafe` code may now assume that besides your own `unsafe` blocks, safe code could never _ever_ accidentally move you - only `unsafe` can do so, and so that's on you as the programmer since, well, `unsafe`. Often `unsafe` code cannot trust safe code [too much](https://doc.rust-lang.org/nomicon/working-with-unsafe.html), but with `Pin<&mut Self>` and `!Unpin`, you may rest assured that all problems (there's ton of problem working with `unsafe`: pointers are [hard](https://github.com/rust-lang/unsafe-code-guidelines/issues/84)) originates right here in your own `unsafe` blocks. Once `Pin`-ed, nobody except you as the author of `T` could move the value.
That's it. `Pin` is created to make `unsafe` code easier to reason about, since again by default safe Rust move things transparently.
And the implementation turns out to be surprisingly simple: introduce `Unpin` _safe_ auto trait s.t. those who actually need the guarantees brought by `!Unpin` to do `unsafe` things must hold a `PhantomPinned` themselves to become `!Unpin`, and for such types we gate all the pointer accesses behind `unsafe`.

While we're on it, we better also do something when our type goes out of scope, i.e. we want to `impl Drop`, since we better somehow clear all the pointers out there: dangling pointers are unwelcomed. I mean they are introduced to point back to us via our own (private) `unsafe` blocks, so there's no way clear them except of course we do this in our own `Drop` implementation.
You may heard that leaking memory being [safe](https://doc.rust-lang.org/1.93.0/std/boxed/struct.Box.html#method.leak) i.e. skipping the `Drop` implementation is safe. Safe code may ackchyually do even more "bizarre" things, like skip the `Drop` _and_ wipe the memory where the value once lived right afterwards: [`ManuallyDrop::new`](https://doc.rust-lang.org/1.93.0/std/mem/struct.ManuallyDrop.html#method.new) is safe!
And this is a trouble. We clean up all the pointers we introduced in the associated methods of which receiver being `Pin<&mut Self>` in our `Drop` implementation. But safe code is free to skip our delicate aftermath routine, WTH?

Once again `Pin` got your back. So your type `T` needs pointer magic. As discussed it's up to you to hold a `PhantomPinned` to make `T: !Unpin`.
Being `!Unpin`, safe code alone have little means constructing `Pin<Ptr: Deref<Target = T>>`; if there's no `Pin<Ptr>`, no "dangerous" method where pointer black magic happen would ever be called, i.e. no `unsafe` code is actually run and all is well - remember that associated functions in which black magic `unsafe` block lives shall have receiver being `Pin<Ptr>`?.
Well you can create a `Pin<Ptr>` for `T: !Unpin` with [pin!](https://doc.rust-lang.org/1.93.0/core/pin/macro.pin.html). But by calling this macro, safe code cannot play the `ManuallyDrop` trick anymore.
You might think you may instead do e.g. [`Pin::new(Box::leak(T::new()))`](https://doc.rust-lang.org/1.93.0/std/marker/trait.Unpin.html#impl-Unpin-for-%26mut+T) to have a `Pin<&mut T>` using only safe Rust. Yes you may skip the `Drop` this way, but ackchyually this is not possible in safe Rust, since `Pin::new` requires `Ptr: Deref<Target: Unpin>`.

How about when `Drop` is skipped [in `unsafe` code](https://doc.rust-lang.org/1.93.0/core/pin/index.html#drop-guarantee)?
Welp that's entirely on you. You created some `Pin<Ptr: Deref<Target = T>>` _via `unsafe`_ since `T: !Unpin`, and you wrote some other `unsafe` code that intentionally skipped some `Drop` implementation, i.e. you broke your own `Pin`/`!Unpin` contract.
Your Rust program is now memory unsafe and `SIGSEGV` ensues.

OTOH when you do not write any `unsafe` code for your `T`, assuming all other APIs are sound (as usual, `unsafe` block, if any, should be sound), you may simply mark your type [`Unpin`](https://docs.rs/tokio/1.49.0/tokio/sync/mpsc/struct.Receiver.html#impl-Unpin-for-Receiver<T>): this is why unlike `Send`/`Sync`, `Unpin` is a _safe_ auto marker trait.

Finally if instead of `struct` - remember, `Future`/FSM and intrusive data structures are, well, some `struct` - you want to write some generic _method_ where you want to do some pointer magic that relies the input paramber to be pinned in place, you'll similarly need to specify your generic type parameter `T` as `T: !Unpin`... except you can't do negative trait bounds as of Rust 1.93.0. So no you cannot take advantage of the invariants what `Pin`/`!Unpin` brings to `unsafe` code authors: you're on your own.

## TL;DR

- `Pin` feels alien simply since you're not supposed to notice nothing when values got moved in Rust
- Except over ther years (mainly with `async`/`Future`) we find that we do at times _need_ to ensure value is pinned in place and its `Drop` always called before the space got repurposed or deallocated.
- `!Unpin` together with `Pin` is for compiler and `unsafe` block wielders to have some way to ensure safe code could never ever move their precious values.
  - `unsafe` is like freedom. You can do everything, and that's why you must be care what (not) to do.
    - Do not move them out via e.g. `std::mem::swap`.
    - Always remember to `Drop` before repurposing the location.
