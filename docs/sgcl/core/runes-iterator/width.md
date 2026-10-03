[sgcl](../../README.md) › [core](../README.md) › [runes](../runes.md) › [iterator](../runes-iterator.md)

# sgcl::runes::iterator::width

```cpp
size_t width() const noexcept;
```

Returns the number of bytes the iterator's code point takes in the text: 1 to 4, 1 for an invalid byte (which is
`utf8::replacement`), 0 at [end](../runes/end.md). The next code point begins at `pos() + width()`.

## Parameters

None.

## Return value

The width of the code point in bytes.

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
    string s = string::concat("aż", '\xFF', "😀");
    for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
        println("U+{:04X}: {}", uint32_t(*it), it.width());
    }
}
```

Output:

```text
U+0061: 1
U+017C: 2
U+FFFD: 1
U+1F600: 4
```

## See also

- [pos](pos.md): the byte position of the code point
- [utf8::width](../utf8/width.md): the width of a code point's encoding
- [sgcl::runes::iterator](../runes-iterator.md)
