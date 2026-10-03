[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::runtime_pattern

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class runtime_pattern;
}
```

A pattern of [format](format.md) that the compiler never saw: a catalogue of translations read from a file when the
program starts, the text of a message chosen by the language of whoever reads it — `"{} left"` in one file and
`"pozostało: {}"` in another. No `consteval` can help there, so the asking moves to where the pattern arrives, and the
forms of [format](format.md), [format_to](format_to.md) and [io::print](../io/print.md) that take one answer whether
it fitted. It is made by [runtime](runtime.md), `txt::runtime(entry)`, or by its constructor.

It keeps the [string](../core/string.md) rather than pointing into it, so a pattern looked up in a table and handed
straight to `format` as a temporary is safe; a string of the library is shared and immutable, so keeping it costs a
pointer and no characters. Where a pattern does not fit, `std::vformat` throws `std::format_error` and Go's
`fmt.Sprintf` writes a complaint into the text (`%!d(string=…)`); here the answer is `nullopt`, and the program
falls back on a pattern of its own.

## Rules

- It holds a `string`, so it lives where a `string` may: on a stack or inside a managed object
  ([the rules of core](../core/README.md#the-rules), 1).
- Nothing is read when it is made: the pattern is read by the call that takes it, every time, and [fits](fits.md)
  asks the question once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](runtime_pattern/runtime_pattern.md) | makes the pattern of a text |
| [view](runtime_pattern/view.md) | the characters of the pattern |
| [text](runtime_pattern/text.md) | the string the pattern keeps |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::runtime_pattern items(string("pozostało: {}"));
    for (int n : {3, 1}) {
        println("{}", txt::format(items, n).value_or("?"));
    }
    println("{}", txt::fits<string>(items));
    return 0;
}
```

Output:

```text
pozostało: 3
pozostało: 1
true
```

## See also

- [runtime](runtime.md): makes one
- [format](format.md), [format_to](format_to.md), [fits](fits.md): what takes one
- [format_pattern](format_pattern.md): the pattern read where the program is compiled
