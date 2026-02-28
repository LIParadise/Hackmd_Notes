# If `T: Send`, _Creating_ `&T` From Any Other Threads is Fine

You might have wondered why we have such `impl`s on [`std::sync::Mutex`](https://doc.rust-lang.org/1.93.0/std/sync/struct.Mutex.html#impl-Send-for-Mutex%3CT%3E), i.e. if `T: Send` then `Mutex<T>: Send + Sync`.

The `Send` part is mostly trivial: per doc,

> `T` must be `Send` for a [`Mutex`](https://doc.rust-lang.org/1.93.0/std/sync/struct.Mutex.html) to be `Send` because it is possible to acquire the owned `T` from the `Mutex` via [`into_inner`](https://doc.rust-lang.org/1.93.0/std/sync/struct.Mutex.html#method.into_inner).

Fair enough.
I mean yet another quick litmus test is the fact we may _obtain `&mut T`_ if we had a `Mutex<T>`, for we may simply [`swap`](https://doc.rust-lang.org/1.93.0/std/mem/fn.swap.html) it out, effectively _moving_ the value to other threads.

But what about the fact `T: Send` implies `Mutex<T>: Sync`?
At first glance I'm stumped. Per the doc, [`Sync`](https://doc.rust-lang.org/1.93.0/std/marker/trait.Sync.html) is:

> Types for which it is safe to share references between threads.

I mean we only said `T: Send`, but with [`scope`](https://doc.rust-lang.org/1.93.0/std/thread/fn.scope.html) or `Box::leak`, `&T` is readily present on indeterminate amount of threads thanks to `Mutex<T>`!
To reiterate, we mentioned only `T: Send`, but this API is saying `Send` _alone_ implies it's entirely fine to create `&T` from _any_ threads.

## What We Want From `Send`

If there's _any_ difference, it's that the references `&T`/`&mut T` we get out of some `&Mutex<T>` have lifetime that are protected by certain [_happens-before_](https://preshing.com/20130702/the-happens-before-relation) [relation](https://preshing.com/20120710/memory-barriers-are-like-source-control-operations).
That is, we have written code that requires the compiler to emit certain [assemblies](https://preshing.com/20131125/acquire-and-release-fences-dont-work-the-way-youd-expect)/[syscalls](https://man7.org/linux/man-pages/man7/futex.7.html) s.t. the OS and the hardware have to ensure for us software programmers that these operations are _mutually exclusive_ and every thread _does_ have certain concensus on what other threads had (not) done, regardless of how heavily parallel the underlying bare metal is.
Or in short, we use [_atomics_](https://sabrinajewson.org/rust-nomicon/atomics/acquire-release.html) and certain syscalls to _protect_ certain things: we have built a _critical section_.

We are using a `Mutex`.
And this is what matters.

We've coded `Mutex<T>: Send` if `T: Send`.
We're saying that if `T` is `Send`, we should be able to get `&mut T` from _any other thread_, simply by passing the `Mutex<T>` to other threads.
I mean this should be a trivial property we want from `Mutex` and `Send` if these two concepts were to make any sense.
We _want_ this to happen; I mean this is what `Mutex` is supposed to be about, visiting certain resource to which we know would be _undefined behavior_ accessing (read or `&T` is always fine, it's write or `&mut T` that matters) concurrently across threads in some controlled way.
But we want _more_. Imagine a `Mutex<T>` of which APIs require _exclusive access_ `&mut Mutex<T>` or even consumes the value i.e. taking `Mutex<T>`, just to get a `&mut T`. I'd imagine if you can prove to the compiler that you have `&mut Mutex<T>`/`Mutex<T>` (in order to get the `&mut T` via this quite limited API), you most likely could have simply used a `&mut T` instead. Or maybe it becomes some sort of [`Arc`][Arc] with blocking behaviors with `Clone` operations.
The point is, Rust lib team ingeniously devised `std::sync::Mutex` with certain atomics/syscalls (which ultimately require [interior mutability](https://doc.rust-lang.org/src/core/cell.rs.html#2297) beknownst to the compiler) s.t. we may get `&mut T` via `&Mutex<T>`.
Again, this `&mut T` is carefully gated behind certain _happens-before_ relations, s.t. `&mut T` _appearing_ on different threads is totally sound: it remains _exclusive_.
Such behavior _is_ what we want from `Send` (or at least what actually is implemented as Rust currently does) as a language concurrency primitive, i.e. getting _exclusive access_ from threads other than the one where `T` was born shall be fine if we said `T: Send`.

To reiterate, being `Send` is about _whether you allow some thread other than where `T` was born having _exclusive_ access to the value_.
So there you have it: as a corollary to such interpretation (definition even), since with `&mut T` you may reborrow and have any number of `&T`, `T: Send` implies you may have any number of `&T` on any number of threads, albeit these `&T` being properly gated within one single thread in a mutually exclusive _happens-before_ relation sense.
Similarly, given `T: Send`, having multiple threads each having access to `&T` is completely sound, as long as those references have lifetimes that are guarded in a memory safe manner behind certain _happens-before_ relations.

It's when you have `&T` _in parallel_ that you require a _different_ guarantee: `Sync`.

## It's All About The Public API

[`std::rc::Rc`](https://doc.rust-lang.org/1.93.0/std/rc/struct.Rc.html#impl-Send-for-Rc%3CT,+A%3E) is neither `Send` nor `Sync` for any `T`.
This is since `Rc` implements [`Clone`](https://doc.rust-lang.org/1.93.0/std/rc/struct.Rc.html#impl-Clone-for-Rc%3CT,+A%3E), so `&Rc<T>` is _as dangerous as_ `Rc<T>`, the latter of which dangerousness ultimately stems from the fact the reference counters are generic PoD and provide no atomic guarantee from the compiler/hardware.

Or back to the `Mutex` example; while the RAII `std::sync::MutexGuard` is `!Send` for [NPTL][NPTL] reasons, the fact it's `Sync` only if `T: Sync` is due to the fact a `&MutexGuard<T>` gives you `&T` directly via [`Deref`](https://doc.rust-lang.org/1.93.0/std/sync/struct.MutexGuard.html#impl-Deref-for-MutexGuard%3C'_,+T%3E).

Or the `Mutex<Rc<T>>` example given in the `Mutex` doc.
If we think in a C/C++ [NPTL][NPTL] viewpoint, this makes total sense.
If you declared some variable that shall only be manipulated when certain lock held, it's almost certainly wrong if you find that exact variable modified without the lock held.
This is exactly the case here: `Rc` is not only pointer to some `T` but also some counter sitting alongside it. That counter may well be modified by other `Rc<T>`.
And Rust encoded this rule into its type system, s.t. you may create `Mutex<Rc<T>>` just fine, but due to the fact it being neither `Send` nor `Sync`, everything ultimately happens on single thread and thus not worth the effort. The program remains sound.

Or [`Arc`](https://mara.nl/atomics/building-arc.html).
Since we'd like [`Arc::get_mut`][Arc] to work, the fact `Arc` is `Clone` means `&Arc<T>` is just as "dangerous" as `Arc<T>`; this plus the actual `Drop` happens on nobody no which thread, it's [`Send + Sync` iff `T: Send + Sync`](https://doc.rust-lang.org/1.93.0/src/alloc/sync.rs.html#273-276).

And finally as [Alice][Alice] mentioned there's also [`sync_wrapper`](https://docs.rs/sync_wrapper/1.0.2/sync_wrapper/struct.SyncWrapper.html):
Having a `&SyncWrapper<T>` is never useful, for it exposes no API taking `&SyncWrapper<T>`, so it's [_always_ `Sync` regardless of `T`](https://docs.rs/sync_wrapper/1.0.2/src/sync_wrapper/lib.rs.html#167) and it's perfectly sound.

# Putting Together

I figured out the _exclusiveness_ part of `Send` myself, then I discovered that [Alice][Alice] had figured out the better, more general definitions, which I'll summarize in my own words as follows:

- `Send` is about whether you allow anything that _requires exclusive access to be done_ to be done _in other threads_
  - ~我也想過過過兒過過的生活~
  - Rust _never_ allows multiple `&mut T`, so automatically we're assuming additionaly that the operations are done in some _mutually exclusive_ way, just like `Mutex`
    - I.e. whether you allow `&mut T` to be present on any other threads besides where `T` had born
  - Move
  - Mutation, which require a `&mut T`
  - [`Drop`](https://doc.rust-lang.org/1.93.0/std/ops/trait.Drop.html): see the `&mut self` [_method receiver_](https://doc.rust-lang.org/1.93.0/std/ops/trait.Receiver.html)?
- `Sync` is about whether you allow for _parallel_ accesses via `&T`
  - Again, the API is what matters: `&T` is [_always_ `Copy`](https://doc.rust-lang.org/1.93.0/src/core/marker.rs.html#494-496) regardless of `T`.
    - There's no way limiting how many pointers to `T` if one `&T` goes across thread boundary. Parallel access to `T` must be sound if you want `&T` to be `Send`.
    - This is why `T: Sync` iff `&T: Send`.
  - You cannot have `mut static` in safe Rust. The language itself ensures available APIs in safe Rust, s.t. `static` global variables requires `Sync` and doesn't have to be `Send`.

And [Alice][Alice] had put together this nice little table (again in my own words):

|                             | `!Send + !Sync` | `Send + !Sync` | `!Send + Sync` | `Send + Sync` | comment                                                              |
| --------------------------- | --------------- | -------------- | -------------- | ------------- | -------------------------------------------------------------------- |
| modify in birth thread      | Ok              | Ok             | Ok             | Ok            | I mean, it's the bare minimum?                                       |
| `&T` parallelly             | No              | No             | Yes            | Yes           | Definition of `Sync`                                                 |
| `&mut T` mutual exclusively | No              | Yes            | No             | Yes           | `Send`: access across threads is fine, provided mutual exclusiveness |
| `&T` mutual exclusively     | No              | Yes            | Yes            | Yes           | Corollary of the last two                                            |
| `&mut T` parallelly         | No              | No             | No             | No            | `&mut T` _must_ be exclusive in Rust                                 |

I guess the take away would be that `T: Send` iff you think letting other threads have `&mut T` remains sound, and `T: Sync` iff you think letting other threads have `&T` remains sound.
I.e. it's ultimately about [mutable xor aliased](https://boats.gitlab.io/blog/post/notes-on-a-smaller-rust/#:~:text=Aliasable,them).

## Examples

- `!Send + Sync`
  - `std::sync::MutexGuard<T>` when `T: Sync`
    - `!Send` is due to [NPTL][NPTL]
    - Being `Deref<Target = T>`, it's `Sync` iff `T: Sync`
- `Send + !Sync`
  - [`std::cell::Cell`](https://doc.rust-lang.org/src/core/cell.rs.html#317-326) when `T: Send`.
    - Consder `&Cell<T>`. Its interior mutability is known to the compiler but not to the hardware. It's not thread-safe.
    - OTOH, `&mut Cell<T>` is basically `&mut T`, so `Cell<T>: Send` if `T: Send`.
- `!Send + !Sync`
  - [pointers](https://doc.rust-lang.org/src/core/marker.rs.html#679-682)
    - This is mostly [simply](https://doc.rust-lang.org/nomicon/send-and-sync.html#send-and-sync) because pointers are inherently [tricky](https://doc.rust-lang.org/1.93.0/std/ptr/index.html) in Rust.
    - > However raw pointers are, strictly speaking, marked as thread-unsafe as more of a _lint_. Doing anything useful with a raw pointer requires dereferencing it, which is already unsafe. In that sense, one could argue that it would be "fine" for them to be marked as thread safe.

[Alice]: https://www.youtube.com/watch?v=eRxqX5_UxaY
[Arc]: https://doc.rust-lang.org/1.93.0/std/sync/struct.Arc.html#method.get_mut
[NPTL]: https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html
