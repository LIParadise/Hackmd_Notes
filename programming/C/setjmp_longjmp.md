# Notes on `x86_64` Frame Pointer `rbp/ebp` and Stack Pointer `rsp/esp`

This note would be using the [AT&T syntax](https://en.wikipedia.org/wiki/X86_assembly_language#Syntax); it's makes more sense to me, since for example `mov %rbp, %rax` sounds much like `cp file_rbp file_rax` in shell syntax.

This tutorial comes directly from [this awesome explanation from Rutgers University](https://www.cs.rutgers.edu/~pxk/419/notes/frames.html). In the following sections I'll just write down some of my personal explanations.

## How a Function Got Called

Consider the following code:

``` c
void
bar(int a, int b){
    int x, y;

    x = 555;
    y = a+b;
}

void
foo(void) {
    bar(111,222);
}
```

And the assembly generated with `gcc -S -m32 <example.c>`

``` assembly=
bar:
    pushl   %ebp
    movl    %esp, %ebp
    subl    $16, %esp
    movl    $555, -4(%ebp)
    movl    12(%ebp), %eax
    movl    8(%ebp), %edx
    addl    %edx, %eax
    movl    %eax, -8(%ebp)
    leave
    ret
foo:
    pushl   %ebp
    movl    %esp, %ebp
    subl    $8, %esp
    movl    $222, 4(%esp)
    movl    $111, (%esp)
    call    bar
    leave
    ret
```

We could visualize the memory/registers using tables:

Assume this below is the state when entering the function `foo(void)`; we're about to execute line `13`. Memory here are just random bits of information; you could think of it being pointer to some `C`-string, if that would make you more comfortable. Comments are in italic for no better representations.

| `ebp`                                                       | `esp`   |
| ----------------------------------------------------------- | ------- |
| `0xffc` *value irrelevant; just check how it's manipulated* | `0xefc` |

| `0xeec`    | `0xef0` | `0xef4`   | `0xef8`  | `0xefc`                                           |
| ---------- | :------ | --------- | -------- | ------------------------------------------------- |
| `PekoMiko` | `I'm`   | `garbage` | `memory` | `0x064` *return address of caller of `foo(void)`* |

Line `13` we see a `pushl`, which would put the first operand to `(-4)(%esp)`, and increment `%esp` by `-4`; effectively *pushing* the first operand on top of the stack, modifying the stack pointer correspondingly. After line `13` before line `14` we would have:

| `ebp`   | `esp`   |
| ------- | ------- |
| `0xffc` | `0xef8` |

| `0xeec`    | `0xef0` | `0xef4`   | `0xef8`                     | `0xefc` |
| ---------- | :------ | --------- | --------------------------- | ------- |
| `PekoMiko` | `I'm`   | `garbage` | `0xffc` *old frame pointer* | `0x064` |

After some stack operations, before the `call bar` in line `18` is executed, we would have memory like below:

| `ebp`                                      | `esp`   |
| ------------------------------------------ | ------- |
| `0xef8` *frame pointers form linked list!* | `0xef0` |

| `0xeec`    | `0xef0` | `0xef4` | `0xef8` | `0xefc` |
| ---------- | :------ | ------- | ------- | ------- |
| `PekoMiko` | `111`   | `222`   | `0xffc` | `0x064` |

To `call` another function, `call` would push current suitable return address onto the stack (and modify the stack pointer correspondingly), after which jump to the specified label. After the `call` on line `18` and before line `2` executed, we would hence have following memory layout.

| `ebp`   | `esp`   |
| ------- | ------- |
| `0xef8` | `0xeec` |

| `0xeec`                                              | `0xef0` | `0xef4` | `0xef8` | `0xefc` |
| ---------------------------------------------------- | :------ | ------- | ------- | ------- |
| `0xabc` *assume it's return address for `foo(void)`* | `111`   | `222`   | `0xffc` | `0x064` |

Similarly to operations above, `bar(int, int)` would do some operations to the memory and registers; a point worth attention is that the parameters are just put on stack in `foo(void)`, and `bar(int, int)` retrieves them accordingly. Before execution of `leave` on line `10`, we would have:

| `ebp`   | `esp`   |
| ------- | ------- |
| `0xee8` | `0xed8` |

| `0xed8` | `0xedc` | `0xee0`                 | `0xee4` | `0xee8` | `0xeec` | `0xef0` | `0xef4` | `0xef8` | `0xefc` |
| ------- | ------- | ----------------------- | ------- | ------- | ------- | :------ | ------- | ------- | ------- |
| `Yagoo` | `ICU`   | `333` *sum of `a`, `b`* | `555`   | `0xef8` | `0xabc` | `111`   | `222`   | `0xffc` | `0x064` |

The `leave` revert the process which we normally do when entering functions:

1. Push old frame pointer onto stack
2. Record current stack pointer to frame pointer

That is,

1. Recover stack pointer via copying from frame pointer
2. Recover frame pointer from that stored on stack, pop the value (increment stack pointer)

Notice the stack is effectively *destroyed* after `leave`. After `leave` on line `10`:

| `ebp`   | `esp`   |
| ------- | ------- |
| `0xef8` | `0xeec` |

| `0xed8` | `0xedc` | `0xee0`                 | `0xee4` | `0xee8` | `0xeec` | `0xef0` | `0xef4` | `0xef8` | `0xefc` |
| ------- | ------- | ----------------------- | ------- | ------- | ------- | :------ | ------- | ------- | ------- |
| `Yagoo` | `ICU`   | `333` *sum of `a`, `b`* | `555`   | `0xef8` | `0xabc` | `111`   | `222`   | `0xffc` | `0x064` |

Notice now the stack pointer points to the return address we set in `foo(void)` i.e. `0xabc`, voilà! Next, `ret` on line `11` uses the return address `0xabc` pointed by `esp` (now containing address `0xeec`) to restore PC, popping the address out (increment stack pointer again); after line `11`:

| `ebp`   | `esp`   |
| ------- | ------- |
| `0xef8` | `0xef0` |

| `0xed8` | `0xedc` | `0xee0` | `0xee4` | `0xee8` | `0xeec` | `0xef0` | `0xef4` | `0xef8` | `0xefc` |
| ------- | ------- | ------- | ------- | ------- | ------- | :------ | ------- | ------- | ------- |
| `Yagoo` | `ICU`   | `333`   | `555`   | `0xef8` | `0xabc` | `111`   | `222`   | `0xffc` | `0x064` |

The `ebp` and `esp` register and relevant memory are just like before we do `call bar` on line `18` in `foo(void)`, and we could continue execution.