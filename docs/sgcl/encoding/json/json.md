[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::json

```cpp
json() noexcept;                              // (1)
json(std::nullptr_t) noexcept;                // (2)
json(bool b) noexcept;                        // (3)
template<class I> json(I v) noexcept;         // (4)
json(double d) noexcept;                      // (5)
json(float f) noexcept;                       // (6)
json(const string& s) noexcept;               // (7)
json(const char* s) noexcept;                 // (8)
json(const slice<const char>& s) noexcept;    // (9)
```

Constructs a value of one of the kinds that need no elements; an array and an object are made by
[array](array.md), [object](object.md) or a [builder](../json-builder.md), or read by [parse](parse.md). None of the
constructors is `explicit`, so a function that takes a `const json&` takes `5`, `true` or `"text"` as they are.

1. Null.
2. Null, from `nullptr`.
3. A boolean.
4. A number of any integer type but `bool` and the character types (`char`, `wchar_t`, `char8_t`, `char16_t`,
   `char32_t`), which are letters rather than numbers: takes part only for those. Held exactly, as an `int64_t`
   when one holds it and as an `uint64_t` past `INT64_MAX`.
5. A number from a double. NaN and the infinities are not JSON numbers: one of them is an assertion in a debug
   build and null in a release one, as `JSON.stringify` writes them.
6. A number from a float: the number of the float's shortest digits, held as the double nearest to it, so
   `json(0.1f)` is `0.1`, equal to `json(0.1)`, and `as<float>()` reads it back as the same float; the same as (5)
   for NaN and the infinities.
7. A string; the value holds `s`'s characters, shared, not copied.
8. A string of the characters of `s`, up to its terminating null.
9. A string of the characters of `s`.

The copy and the move are the implicit ones and copy the handle: the value they make shares everything with
`other`, which never changes.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the boolean |
| `v` | the integer |
| `d`, `f` | the number; finite |
| `s` | the characters of the string, UTF-8 |

## Complexity

- (1–5), (7) Constant.
- (6) Constant: the float's shortest digits written and read once.
- (8–9) Linear in the length of `s`.

## Exceptions

None.

## Notes

A string's characters are not checked when the value is made: invalid UTF-8 is kept and written as U+FFFD by
[to_string](to_string.md).

A float in a json is written with its own shortest digits, `0.1` for `0.1f`, as a float field of a program's type
is ([stringify](stringify.md)); the double it widens to, `json(double(0.1f))`, is written
`0.10000000149011612`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json values[] = {
        encoding::json(),
        nullptr,
        true,
        42,
        uint64_t(18446744073709551615u),
        2.5,
        0.1f,
        string("text"),
        "C string",
    };
    for (auto& v : values) {
        println(v.to_string());
    }
}
```

Output:

```text
null
null
true
42
18446744073709551615
2.5
0.1
"text"
"C string"
```

## See also

- [array](array.md), [object](object.md): a value with elements or members
- [parse](parse.md): a value read from a text
- [sgcl::encoding::json](../json.md)
