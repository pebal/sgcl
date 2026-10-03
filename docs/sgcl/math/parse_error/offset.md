[sgcl](../../README.md) › [math](../README.md) › [parse_error](../parse_error.md)

# sgcl::math::parse_error::offset

```cpp
size_t offset() const noexcept;
```

The byte of the text where the reading stopped:

- 0 for an empty text;
- the end of the text when there was only a sign, or nothing after a slash or an `e`;
- the first byte that is not a digit of the base, for a letter past ASCII the byte its encoding starts at;
- for a [rational](../rational.md), the first digit of a denominator of zero, and the first byte after the `e` of
  an exponent past a million.

## Parameters

None.

## Return value

The offset in bytes from the start of the text.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (const char* text : {"", "-", "12x4", "ff"}) {
        println("\"{}\" stops at {}", text, math::big_integer::parse(text).error().offset());
    }
    println("{}", math::rational::parse("3/0").error().offset());
    println("{}", math::rational::parse("1e9999999").error().offset());
}
```

Output:

```text
"" stops at 0
"-" stops at 1
"12x4" stops at 2
"ff" stops at 0
2
2
```

## See also

- [message](message.md): the sentence
- [sgcl::math::parse_error](../parse_error.md)
