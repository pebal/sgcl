[sgcl](../../README.md) › [core](../README.md) › [runes](../runes.md) › [iterator](../runes-iterator.md)

# sgcl::runes::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position of the iterator's code point in the text: the `i` of Go's `for i, r := range s`. The
position of [end](../runes/end.md) is the size of the text.

## Parameters

None.

## Return value

The position of the first byte of the code point.

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
    string s = "aż €";
    for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
        println("{}: U+{:04X}", it.pos(), uint32_t(*it));
    }
}
```

Output:

```text
0: U+0061
1: U+017C
3: U+0020
4: U+20AC
```

## See also

- [width](width.md): the bytes the code point takes
- [sgcl::runes::iterator](../runes-iterator.md)
