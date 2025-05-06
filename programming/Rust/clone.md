[stackoverflow](https://stackoverflow.com/questions/56464300)

Note that the `Clone` trait is in no way special to the compiler.
Thus when one calls `Clone::clone`, the implementor has no choice but painstakingly allocate everything for it returns `Self`, while OTOH for `Clone::clone_from`, since we already have a `&mut self`, we may reuse some of its resources.

See also [`Vec`](https://github.com/rust-lang/rust/blob/f5d3fe273b8b9e7125bf8856d44793b6cc4b6735/library/alloc/src/slice.rs#L811-L825) implementation and [`Waker`](https://doc.rust-lang.org/std/task/struct.Waker.html#impl-Clone-for-Waker) documentation.
