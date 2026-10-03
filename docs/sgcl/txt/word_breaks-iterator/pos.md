[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](../word_breaks.md) › [iterator](../word_breaks-iterator.md)

# sgcl::txt::word_breaks::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position of the iterator's element in the text: where a segment between two word boundaries begins.
The position of [end](../word_breaks/end.md) is the size of the text.

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
    string s = "Ala ma kota";
    txt::word_breaks parts(s);
    for (auto it = parts.begin(); it != parts.end(); ++it) {
        print("{} ", it.pos());
    }
    println();
}
```

Output:

```text
0 3 4 6 7 
```

## See also

- [size](size.md): the bytes the element takes
- [sgcl::txt::word_breaks::iterator](../word_breaks-iterator.md)
