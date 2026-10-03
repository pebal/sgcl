[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::count

```cpp
static constexpr size_t count(std::string_view s) noexcept;
```

Returns the number of code points of `s`, an invalid byte counting as one, as [decode](decode.md) reads it. A run of
ASCII is a code point a byte and is counted eight bytes at a time; the rest is decoded a code point at a time, which
is why the continuation bytes cannot simply be counted out instead.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the bytes of the text |

## Return value

The number of code points.

## Complexity

Linear in `s.size()`.

## Exceptions

None.

## Notes

A string's `rune_count()` ([mixin::text](../mixin/text.md)) is this over its bytes. The cost a byte, against Go's
`utf8.RuneCountInString`, is on [Benchmarks: Text](../benchmarks.md#text).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", utf8::count("żółw"), utf8::count("hello"));
    println("{}", utf8::count("a\xFF" "b"));  // the invalid byte is one
    string s = "Łódź 😀";
    println("{} bytes, {} code points", s.size(), s.rune_count());
}
```

Output:

```text
4 5
3
12 bytes, 6 code points
```

## See also

- [runes](../runes.md): the code points themselves
- [ascii_run](ascii_run.md): the runs counted eight bytes at a time
- [sgcl::utf8](../utf8.md)
