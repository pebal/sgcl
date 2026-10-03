[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::starts_rune

```cpp
static constexpr bool starts_rune(char b) noexcept;
```

Checks whether the byte `b` begins a sequence: an ASCII byte or a leading byte, not a continuation byte (`10xxxxxx`).
A position whose byte begins a sequence is a place where a text may be cut without cutting a code point in two.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the byte |

## Return value

`true` unless `b` is a continuation byte.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "zażółć";
    size_t cut = 5;  // inside ó
    while (cut > 0 && !utf8::starts_rune(s[cut])) {
        --cut;
    }
    println("{} {}", cut, s.substr(0, cut));
}
```

Output:

```text
4 zaż
```

## See also

- [decode_last](decode_last.md): the code point before a position
- [sgcl::utf8](../utf8.md)
