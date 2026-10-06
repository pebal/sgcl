[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::cbor

```cpp
cbor() noexcept;                              // (1)
cbor(std::nullptr_t) noexcept;                // (2)
cbor(bool b) noexcept;                        // (3)
template<class I> cbor(I v) noexcept;         // (4)
cbor(double d) noexcept;                      // (5)
cbor(float f) noexcept;                       // (6)
cbor(const string& s) noexcept;               // (7)
cbor(const char* s) noexcept;                 // (8)
cbor(const math::big_integer& v) noexcept;    // (9)
```

Constructs a value of what C++ has a literal for; the rest is made by the static functions
([array](array.md), [map](map.md), [bytes](bytes.md), [tagged](tagged.md)…) or read by [parse](parse.md). None is
`explicit`, so `map({{"a", 1}})` and a function taking a `const cbor&` take `1`, `true` or `"text"` as they are.

1. null.
2. null, of `nullptr`.
3. A boolean.
4. An integer of any integral type but `bool` and the characters: takes part only for those.
5. A float.
6. A float of the float's value; written as a single or a half when it holds it.
7. A text string; its characters shared, not copied.
8. A text string of the characters up to the null.
9. An integer of any size: within -2^64 to 2^64 - 1 an integer, past it a bignum, tag 2 or 3 over its bytes.

The copy and the move are the implicit ones and copy the handle; the value never changes.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the boolean |
| `v` | the integer |
| `d`, `f` | the float |
| `s` | the text, UTF-8 |

## Complexity

- (1–8) Constant; (8) linear in the length of `s`.
- (9) Linear in the size of `v`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (encoding::cbor c : {encoding::cbor(), encoding::cbor(true), encoding::cbor(-10), encoding::cbor(1.5),
                             encoding::cbor("IETF"), encoding::cbor(math::big_integer(1) << 64)}) {
        println("{} -> {}", c.to_string(), encoding::hex::encode(c.to_bytes()));
    }
}
```

Output:

```text
null -> f6
true -> f5
-10 -> 29
1.5 -> f93e00
"IETF" -> 6449455446
2(h'010000000000000000') -> c249010000000000000000
```

## See also

- [parse](parse.md)
- [array](array.md), [map](map.md)
- [sgcl::encoding::cbor](README.md)
