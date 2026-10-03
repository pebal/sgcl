[sgcl](../../README.md) › [core](../README.md) › [runes](../runes.md)

# sgcl::runes::runes

```cpp
/*(1)*/ runes() noexcept = default;
/*(2)*/ explicit runes(const slice<const char>& text) noexcept;
```

Constructs the range of the code points of a text.

1. An empty range, over no text.
2. The code points of `text`, a slice of UTF-8 bytes; the range keeps the slice, and with it the object the bytes lie
   in.

A string's or a text slice's `runes()` ([mixin::text](../mixin/text.md)) makes (2) over its own bytes; the
constructor is for a slice at hand, a piece of a buffer as much as a piece of a string.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the bytes of the text |

## Complexity

Constant: nothing is decoded until the walk.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    runes none;
    println("{}", none.empty());

    string line = "key=wartość";
    runes value(line.as_slice(4));
    println("{} code points in {} bytes", value.count(), value.text().size());
}
```

Output:

```text
true
7 code points in 9 bytes
```

## See also

- [text](text.md): the slice the range walks
- [sgcl::runes](../runes.md)
