[sgcl](../../README.md) › [core](../README.md) › [runes](README.md)

# sgcl::runes::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../runes-iterator/README.md) past the last code point: at the byte position equal to the size of the
text. It is not dereferenced.

## Parameters

None.

## Return value

The iterator past the last code point.

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
    string s = "łąka";
    runes letters = s.runes();
    size_t n = 0;
    for (auto it = letters.begin(); it != letters.end(); ++it) {
        ++n;
    }
    println("{} code points, the end at byte {}", n, letters.end().pos());
}
```

Output:

```text
4 code points, the end at byte 6
```

## See also

- [begin](begin.md): an iterator to the first code point
- [sgcl::runes](README.md)
