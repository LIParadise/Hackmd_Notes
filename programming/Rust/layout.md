[Rust reference: Layout](https://doc.rust-lang.org/reference/type-layout.html#type-layout)

[Rust reference: `#[repr(C)]`](https://doc.rust-lang.org/reference/type-layout.html#reprc-enums-with-fields)

For a Rust `#[repr(C)]` enum _with fields_, the representation is a `#[repr(C)]` struct with two fields, one field-less `#[repr(C)]` Rust enum field (thus with C native enum size), and one `#[repr(C)]` union, each variant of which is itself a generic `#[repr(C)]` struct: the structs composing the union contain no tag information.

```rust
// This Enum has the same representation as ...
#[repr(C)]
enum MyEnum {
    A(u32),
    B(f32, u64),
    C { x: u32, y: u8 },
    D,
 }

// ... this struct.
#[repr(C)]
struct MyEnumRepr {
    tag: MyEnumDiscriminant,
    payload: MyEnumFields,
}

// This is the discriminant enum.
#[repr(C)]
enum MyEnumDiscriminant { A, B, C, D }

// This is the variant union.
#[repr(C)]
union MyEnumFields {
    A: MyAFields,
    B: MyBFields,
    C: MyCFields,
    D: MyDFields,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct MyAFields(u32);

#[repr(C)]
#[derive(Copy, Clone)]
struct MyBFields(f32, u64);

#[repr(C)]
#[derive(Copy, Clone)]
struct MyCFields { x: u32, y: u8 }

// This struct could be omitted (it is a zero-sized type), and it must be in
// C/C++ headers.
#[repr(C)]
#[derive(Copy, Clone)]
struct MyDFields;

```

[Rust reference: `#[repr(u*)]`/`#[repr(i*)]`](https://doc.rust-lang.org/reference/type-layout.html#primitive-representation-of-enums-with-fields)

For a Rust `#[repr(u8)]` enum with fields, the representation is a `#[repr(C)]` union, each variant of which is itself a `#[repr(C)]` struct _with tag information_: the tag is the first field, a field-less `#[repr(u8)]` Rust enum, followed by other associated fields.

```rust
// This enum has the same representation as ...
#[repr(u8)]
enum MyEnum {
    A(u32),
    B(f32, u64),
    C { x: u32, y: u8 },
    D,
 }

// ... this union.
#[repr(C)]
union MyEnumRepr {
    A: MyVariantA,
    B: MyVariantB,
    C: MyVariantC,
    D: MyVariantD,
}

// This is the discriminant enum.
#[repr(u8)]
#[derive(Copy, Clone)]
enum MyEnumDiscriminant { A, B, C, D }

#[repr(C)]
#[derive(Clone, Copy)]
struct MyVariantA(MyEnumDiscriminant, u32);

#[repr(C)]
#[derive(Clone, Copy)]
struct MyVariantB(MyEnumDiscriminant, f32, u64);

#[repr(C)]
#[derive(Clone, Copy)]
struct MyVariantC { tag: MyEnumDiscriminant, x: u32, y: u8 }

#[repr(C)]
#[derive(Clone, Copy)]
struct MyVariantD(MyEnumDiscriminant);

```

[Rust reference: `#[repr(C, u*)]`/`#[repr(C, i*)]`](https://doc.rust-lang.org/reference/type-layout.html#combining-primitive-representations-of-enums-with-fields-and-reprc)

This is basically `#[repr(C)]` enum, i.e. a struct containing a tag and an union (instead of union of which variants come with embedded tag), except the tag field is explicitly specified: instead of implicitly following the C generic enum representation (field-less enum with `#[repr(C)]` attribute), it's instead explicitly specified as such (field-less enum with `#[repr(u*)]`/`#[repr(i*)]` attribute).
