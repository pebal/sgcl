[sgcl](../../README.md) › [txt](../README.md) › [graphemes](../graphemes.md) › [iterator](../graphemes-iterator.md)

# sgcl::txt::graphemes::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position of the iterator's element in the text: where a grapheme cluster begins. The position of
[end](../graphemes/end.md) is the size of the text.

## Parameters

None.

## Return value

The position of the first byte of the element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "zażółć";
    txt::graphemes all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.pos());
    }
    println();
}
```

Output:

```text
0 1 2 4 6 8 
```

## See also

- [size](size.md): the bytes the element takes
- [sgcl::txt::graphemes::iterator](../graphemes-iterator.md)
