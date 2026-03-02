# Pointers Are Tricky

If all you do is math on `u64`, compiler is very willing to optimize away any _unnecessary_ calculations - some calculations carry side effects that compiler cannot ignore, e.g. division by zero, `volatile`, or certain atomic operations, but let's not talk about that today. Anyhow, the snippet below simply [returns `5050`](https://godbolt.org/z/oxb3s4P94). Desugaring the iterators a bit, it's saying create a local `0` on the _stack_, add all those _intermediates_ (in RISC-V `lui` sense i.e. scalars) together, then return that number. But compilers are smart enough to simply pre-calculate these for us since there's no possible runtime side effects.

```rust
fn foo() -> u64 {
    (1..=100).into_iter().sum()
}
```

But then we'd like to use pointers.
Turns out pointers are so unfriendly to compilers, since the store/load could be anywhere.
In fact you might have tripped on this exact problem before, when you first learned the power of `XOR`/`^` operator in C:

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
void swap_int32(int32_t* p, int32_t* q) {
    // XOR is fabulous!
    *p = *p ^ *q;
    *q = *p ^ *q;
    *p = *p ^ *q;
}
int main() {
    static const int32_t NOICE = 42069;
    int32_t noice = NOICE;
    swap_int32(&noice, &noice);
    assert(NOICE == noice); // fails
}
```

This is since _pointer aliasing_. If you're interested in computer architecture you've probably learned that VIPT cache are somewhat limited in size for after certain threshold you'll need to take aliasing into account, thus complicating the design. The point is, you got some (virtual address) pointers, and turns out some of them turns out to be pointing to the same exact (physical) address.
Here the problem is we expect to have one _original value_ and one _aggregate value_ produced by `XOR` _at the same time_, in order to extract the original information, but the pointers _aliased_ s.t. there's only one location, and everything decays to zero.
You may or may not came across keyword `restrict` and that `memcpy` is higher performance than `memmove`.

As such, compilers are forbidden by doing what it's designed to do, i.e. optimizing programs. If it pessimistically assume pointer writes always alias with other pointers or even local variables, after each pointer write, it has no choice but to emit the instructions to reload every value back to the ISA registers, and every write would serve as a _compiler barrier_ disallowing reorderings, which tanks performance.

This is why C11 coded that _objects_ may only be referred to by certain _lvalue expressions_ (6.5, paragraph 7):

> - a type compatible with the effective type of the object,
> - a qualified version of a type compatible with the effective type of the object,
> - a type that is the signed or unsigned type corresponding to the effective type of the
> object,
> - a type that is the signed or unsigned type corresponding to a qualified version of the
> effective type of the object,
> - an aggregate or union type that includes one of the aforementioned types among its
> members (including, recursively, a member of a subaggregate or contained union), or
> - a character type

Honestly I'm too digesting this list, but one of the main take away is that for example two `uint32_t*` are assumed to be aliasing with each other, and `char*` is _always_ pessimistically assumed to be aliasing. So far, so good: compilers seem to be simply doing what exactly we told them to do, assuming aliasing; that `swap_int32` might go "wrong" is simply we passed aliasing pointers in, so that's on us.

Well, until compilers _seem_ to ignore what we told 'em to do. I bet when you learned IEEE754 floating point and endianess of machines you've tried something like [this](https://godbolt.org/z/Ga7TE1TPj) (modified from [here](https://stackoverflow.com/a/98356/9933842)):

```c
#include <stdint.h>

/// strict aliasing not taken advantage of by many modern compilers,
/// since the C standard is not exactly the mental model behind many programmers and code bases,
/// so to be "correct", "compatible", and "actually work",
/// most versions of both `gcc` and `clang` _do_ write the bit mask.
float modern_tbaa_workaround (float* p_f)
{
    uint32_t* temp = (uint32_t*)p_f;
    *temp &= 0x7fffffff;
    return *p_f;
}

/// `float` and `uint32_t` are entirely different types,
/// thus the compiler may well assume these two pointers don't alias,
/// meaning the pointee of the (pointer to float) most likely won't be cleared and `2.0` is returned:
/// type-punning is dangerous exactly because some point deep in the call stack,
/// the compiler is legally _not_ doing what we "told" them to do!
/// 
/// toggle between `-fno-strict-aliasing` and `-fstrict-aliasing` to convince yourself.
float ub(float* p_f, uint32_t* p_u) {
    *p_f = -2.0;
    *p_u &= 0x0;
    return *p_f;
}
```

This is _undefined behavior_: the C11 list above means that a `float` must not be accessed by `unsigned int*`, so the compiler is free to ignore the write via `unsigned int*`. To an optimizing compiler utilizing the _strict aliasing rule_, this is basically a _dead store_, in particular the `float` would be returned as-is.

## With Great Power Comes Great Responsibility

Turns out [this](https://stackoverflow.com/questions/98650) is UB in C.

```c
// Source - https://stackoverflow.com/a/99010
// Posted by Doug T., modified by community. See post 'Timeline' for change history
// Retrieved 2026-03-01, License - CC BY-SA 4.0

typedef struct Msg
{
    unsigned int a;
    unsigned int b;
} Msg;

void SendWord(uint32_t);

int main(void)
{
    // Get a 32-bit buffer from the system
    uint32_t* buff = malloc(sizeof(Msg));
    
    // Alias that buffer through message
    Msg* msg = (Msg*)(buff);
    
    // Send a bunch of messages    
    for (int i = 0; i < 10; ++i)
    {
        msg->a = i;
        msg->b = i+1;
        SendWord(buff[0]);
        SendWord(buff[1]);   
    }
}
```
