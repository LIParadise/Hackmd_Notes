---
tags: C, programming, C++
---

# The C/C++ `switch` statement

References and quotes are from [this awesome doc from Microsoft](https://docs.microsoft.com/en-us/cpp/c-language/switch-statement-c?view=msvc-160).

## I'm familiar with `switch` already, so what?

Presumably you're familiar with this specific style of `switch`:

```c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv){
    if( argc != 2)
        return 1;
    
    int user_input = atoi(argv[1]);
    int ret = 0;
    switch( user_input ){
        case 1:
            printf( "First\n" );
            break;
        case 2:
            printf( "TWO\n" );
            break;
        case 69:
            printf( "Noice\n" );
            break;
        default:
            printf( "Peko...\n" );
            ret = 1;
            break;
    }
    
    return ret;
}
```

Which naturally corresponds to the following equivalent:

```C
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv){
    if( argc != 2)
        return 1;
    
    int user_input = atoi(argv[1]);
    int ret = 0;
    if( user_input == 1 ){
        printf( "First\n" );
    }else if ( user_input == 2 ){
        printf( "TWO\n" );
    }else if ( user_input == 69 ){
        printf( "Noice\n" );
    }else{
        printf( "Peko...\n" );
        ret = 1;
    }
    
    return ret;
}
```

So then, why spend time illustrate this?

## `C/C++` syntax is much like `asm`

`switch` and `break` are just like `JAL` with `rd == x0` in RISC-V assembly or `JMP` in x86_64, and the `case` are like the tags in assembly!

> Execution of the `switch` statement body begins at the first statement in or after the matching `labeled-statement`. Execution proceeds until the end of the body, or until a `break` statement transfers control out of the body.

With this understanding, this following code would be less confusing:

```c
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char** argv ){
    if( argc != 2 )
        exit(1);

    int user_input = atoi(argv[1]);
    switch(user_input){
        case 420:
            user_input--;
            printf( "marijuanas; user_input = %d\n", user_input );
        default:
            user_input = 68;
            printf( "default; user_input = %d\n", user_input );
        case 69:
            printf( "Noice(?); break\n" );
            break;
    }

    return 0;
}
```

When `./a.out 69`, we would have:

```
Noice(?); break
```

This spefic case is intuitive to unserstand, but when `x` is `420`:

```
marijuanas; user_input = 419
default; user_input = 68
Noice(?); break
```

And when `x` is any number other than `420` or `69`:

```
default; user_input = 68
Noice(?); break
```

Seems kinda absurd, right? `user_input` is not `69`, but `Noice` is present in output!

This is because what `switch` do is *transfer control to specific tag*

> A `switch` statement causes control to transfer to one `labeled-statement` in its statement body, depending on the value of `expression`.

After the control is transferred to the specific `case`, the expression used in `switch()` and the cases in `switch` body are just *irrelevant*; they only serve as landmarks for the `switch(user_input)` to jump!

Hence, when situation like `user_input == 42069`, when computer sees the `switch`, it would think, "*I got 3 places to jump to; it's not `420`, it's not `69`, so it's `default` to jump to!*" and jumps there. After printing `default; user_input = 68`, the computer just *don't care* if `user_input == 69` as seemly suggested by `case 69`; `case 69` is compiled to be a tag in assembly to jump to, and have *no* semantics when we're not jumping! Since it's just a tag in assembly, computer would just continue the execution like it's not there.

Below is possible corresponding assembly output; comment added with `#` for clarification:

```assembly
	.file	"switch.c"
	.option nopic
	.attribute arch, "rv64i2p0_m2p0_a2p0_f2p0_d2p0_c2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"marijuanas; user_input = %d\n"
	.align	3
.LC1:
	.string	"default; user_input = %d\n"
	.align	3
.LC2:
	.string	"Noice(?); break"
	.section	.text.startup,"ax",@progbits
	.align	1
	.globl	main
	.type	main, @function
main:
	addi	sp,sp,-16
	sd	ra,8(sp)
	li	a5,2
	bne	a0,a5,.L7
	ld	a0,8(a1)
	call	atoi
	li	a5,69
	beq	a0,a5,.L3
	li	a5,420
	beq	a0,a5,.L8
.L4:
# `default`
# notice the 2 `beq` above
	lui	a0,%hi(.LC1)
	li	a1,68
	addi	a0,a0,%lo(.LC1)
	call	printf
.L3:
# `case 69`
	lui	a0,%hi(.LC2)
	addi	a0,a0,%lo(.LC2)
	call	puts
	ld	ra,8(sp)
	li	a0,0
	addi	sp,sp,16
	jr	ra
.L8:
# `case 420`
	lui	a0,%hi(.LC0)
	li	a1,419
	addi	a0,a0,%lo(.LC0)
	call	printf
	j	.L4
.L7:
	li	a0,1
	call	exit
	.size	main, .-main
	.ident	"GCC: (Arch Linux Repositories) 10.2.0"

```

```bash
$ riscv64-elf-gcc --version
riscv64-elf-gcc (Arch Linux Repositories) 10.2.0
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

$ riscv64-elf-gcc -O2 -S -o switch.s switch.c
```