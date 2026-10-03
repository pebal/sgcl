[sgcl](../../README.md) › [txt](../README.md) › [format_pattern](../format_pattern.md)

# sgcl::txt::format_pattern\<A...\>::format_pattern

```cpp
template<class S>
requires std::is_convertible_v<const S&, std::string_view>
consteval format_pattern(const S& text);
```

Reads `text` as a pattern of [format](../format.md) for values of the types `A...`, where the program is compiled.
One pass does both: every field is asked whether the value it names takes its
[specification](../format.md#the-specification), and every step is written down so that nothing has to be read again
when the program runs. Not explicit: a literal written where a pattern is expected converts by itself.

A pattern that does not fit the values — a brace left open, `{2}` of two values, `{:.3}` of an integer, `{:d}` of a
name, `{:%Q}` of a time — stops the compilation, the message of the compiler pointing into this constructor and the
pattern in the call above it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the pattern, a constant: a literal or a `constexpr` view |

## Complexity

Linear in the length of `text`, where the program is compiled; nothing where it runs.

## Exceptions

None where the program runs: a pattern that does not fit is an error of the compiler.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr txt::format_pattern<int, string> row("{:>4} {}");
    println("{}", txt::format(row, 7, string("seven")));
    println("{}", txt::format(row, 12, string("twelve")));
    return 0;
}
```

Output:

```text
   7 seven
  12 twelve
```

## See also

- [view](view.md): the text it was made of
- [sgcl::txt::format_pattern](../format_pattern.md)
