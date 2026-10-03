[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_allowed_identifier

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool is_allowed_identifier(const string& text) noexcept;
}
```

Checks whether every code point of the text is one [UTS #39](https://www.unicode.org/reports/tr39/) allows in an
identifier ([identifier_status_of](identifier_status_of.md)). It says nothing about the scripts they are written in:
that is [restriction_level_of](restriction_level_of.md). An empty text is not allowed: an empty name is not a name.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8; an invalid byte is `U+FFFD`, which is restricted |

## Return value

`true` when the text is not empty and every code point of it is allowed.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

**Nothing here looks at the bidirectional algorithm.** A name with a right to left override in it can be drawn in an
order its bytes do not have; [bidi_runs](bidi_runs/README.md) is where that is asked about, and `is_allowed_identifier`
refuses the overrides because UTS #39 does, not because it reasons about them.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"wartość", "na me", ""}) {
        println("[{}] {}", s, txt::is_allowed_identifier(s));
    }
    println("with an override: {}", txt::is_allowed_identifier("a\u202Eb"));
}
```

Output:

```text
[wartość] true
[na me] false
[] false
with an override: false
```

## See also

- [identifier_status_of](identifier_status_of.md): one code point
- [restriction_level_of](restriction_level_of.md): the scripts of a name
- [txt](README.md)
