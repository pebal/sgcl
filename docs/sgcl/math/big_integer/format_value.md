[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::format_value (sgcl::math::big_integer)

```cpp
void format_value(txt::format_sink& out, const big_integer& v,
                  const txt::format_spec& spec) noexcept;
```

What [txt::format](../../txt/format.md) writes for a `big_integer`, found by it beside the type: the number as
`std::format` writes an `int`. `{}` and `{:d}` are decimal, `{:x}` `{:X}` `{:o}` `{:b}` `{:B}` the other bases,
`#` the prefix (`0x`, `0X`, `0b`, `0B`, or a leading `0` in octal for a number that is not zero), `+` and a space
the sign of a positive number, and the width, fill and alignment of any field, the number to the right unless the
field says otherwise; the zeros of `{:040}` go after the sign and the prefix. A negative number in another base is
its magnitude after a minus, `-ff`. What Go's `fmt` does with `%d`, `%x`, `%o` and `%b`, with the specification
checked where the pattern is compiled.

A specification a whole number does not take — `{:.3}`, `{:f}`, `{:c}` — is an error of the compiler in a literal
pattern, and a pattern read when the program runs ([txt::runtime](../../txt/format.md)) gives `nullopt`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `v` | the number written |
| `spec` | the field's specification, as `txt::format` read it |

## Return value

None.

## Complexity

That of [to_string](to_string.md) in the base, the digits taken from the limbs with no string made for them, and
linear in the width.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    math::big_integer a = math::big_integer(2).pow(100);
    println("{:#x} {:b}", a, math::big_integer(10));
    math::big_integer n = 42;
    println("[{:+}] [{:>8}] [{:*^9}] [{:#010x}]", n, n, n, -n);
    println("{:040}", -a);
    println("{}", txt::format(txt::runtime("{:.3}"), a).has_value());
}
```

Output:

```text
0x10000000000000000000000000 1010
[+42] [      42] [***42****] [-0x000002a]
-000000001267650600228229401496703205376
false
```

## See also

- [to_string](to_string.md): the digits in a base, and the stream
- [txt::format](../../txt/format.md): the patterns and their specifications
- [sgcl::math::big_integer](../big_integer.md)
