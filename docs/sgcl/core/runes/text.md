[sgcl](../../README.md) › [core](../README.md) › [runes](README.md)

# sgcl::runes::text

```cpp
const slice<const char>& text() const noexcept;
```

Returns the slice of the text the range walks: its bytes, and the object they lie in, which the slice holds. A
position an iterator gives by `pos()` is a position in this slice.

## Parameters

None.

## Return value

A reference to the slice of the text, valid while the range lives.

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
    string s = "cena: 5 €";
    runes r = s.runes();
    for (auto it = r.begin(); it != r.end(); ++it) {
        if (*it == U'€') {
            println("[{}] [{}]", r.text().substr(0, it.pos()), r.text().substr(it.pos()));
        }
    }
}
```

Output:

```text
[cena: 5 ] [€]
```

## See also

- [(constructor)](runes.md): the range over a slice
- [sgcl::runes](README.md)
