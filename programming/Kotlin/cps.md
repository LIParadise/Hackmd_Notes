# The CPS concurrency in Kotlin

`suspend fun` becomes some anonymous class that `implements Contiuation` of which an instance method is `invokeSuspend`, inside which is the state machine version of the original body. For each state (something along with `when(label)`), roughly it's the following things:

1. Restore local (stack) variables via its own instance variables
2. Switch the state
    - Yes, switch the state first: most of the time `invokeSuspend` is called iff the `suspend fun` which returned `COROUTINE_SUSPENDED` in the first place just resolved, i.e. the _continuation passing style_ CPS callback just got invoked, in which case some value would get returned via `Continuation.resumeWith` (in fact `public final override fun BaseContinuationImpl.resumeWith`)
        - `suspendCoroutine` is a bit tricky: it relies on these two facts:
            - [`kotlin.coroutines.intrinsics.suspendCoroutineUninterceptedOrReturn`](https://github.com/JetBrains/kotlin/blob/c448af19ded1b1a4e96e9af6412cd9acb100ce1a/libraries/stdlib/src/kotlin/coroutines/intrinsics/Intrinsics.kt#L41)
            - [`kotlin.coroutines.SafeContinuation`](https://github.com/JetBrains/kotlin/blob/f2fa048b832148119b74e68d3cf89bf94760ad65/libraries/stdlib/jvm/src/kotlin/coroutines/SafeContinuationJvm.kt)
            1. `suspendCoroutineUninterceptedOrReturn`, being compiler intrinsic, is able to get us the raw `Continuation`
            2. `suspendCoroutine` wraps the "raw" (in the sense that it's probably `BaseContinuationImpl`) in `SafeContinuation`, in particular its `resumeWith` is not the loop-unroll of `BaseContinuationImpl` but some thread-safe oneshot operation.
            3. Being thread-safe, `SafeContinuation` either returns `COROUTINE_SUSPENDED` or some arbitrary value, the latter e.g. because the block calls `Continuation.resumeWith` somehow.
                - Thus if you `val t = Thread { it.resume("hehe") }; t.start(); t.join();` in the block, it's _not_ the newly spawned thread who travels down the list of CPS callback: that's the behavior of `BaseContinuationImpl`, which is overriden by `SafeContinuation`.
                    - `SafeContinuation.getOrThrow` is a thread-safe getter, returning either `COROUTINE_SUSPENDED` or the yielded value, which becomes the return value on which `suspendCoroutineUninterceptedOrReturn` is based.
                - Which is if one somehow `Continuation.resumeWith` before `SafeContinuation.getOrThrow`, it's as if the `suspendCoroutine` didn't suspend nothing, but just yield some value just like some generic synchronous function call.
3. Pass `this` as the `Continuation` to the subsequent `suspend fun` call: continuation passing style, CPS.
    - The rabbit hole deepens, till some `suspend fun foo` calls `suspend fun bar`, where `bar` is a wrapper around some I/O operation: the anonymous instance of this invocation of `bar` has a chance storing the caller instance `foo` somewhere. Say `bar`'s body were just `suspendCoroutine { myGlobalContinuationSingleton = it }`.
    - Later, when the I/O operation is done, some thread calls `myGlobalContinuationSingleton.resume(myIoData)`: the thread calls `BaseContinuationImpl.resumeWith`, which calls the caller instance `foo`'s `invokeSuspend`, and in a loop recursion manner calling the ancestors.
        - This way our choice of always switch the state of the state machine is justified in `invokeSuspend` before calling the next `suspend fun` is justfied: the next time `invokeSuspend` is called, it's because the CPS stored in the callee is unrolling via the loop recursion.
        - See also `kotlin.coroutines.jvm.internal.BaseContinuationImpl.resumeWith`
    - Now that this new invocation of `invokeSuspend` is passed with some `Result` of last execution, now we do the same thing: check the `Result` if to `throw`, switch the state, and do CPS again: the leaf `suspend fun` would store the `Continuation`/`BaseContinuationImpl` somewhere and "wake" us later: CPS is callback style!
