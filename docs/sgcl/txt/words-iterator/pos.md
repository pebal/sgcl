[sgcl](../../README.md) › [txt](../README.md) › [words](../words.md) › [iterator](../words-iterator.md)

# sgcl::txt::words::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position of the iterator's element in the text: where a word begins. The position of
[end](../words/end.md) is the size of the text.

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
    string s = "Ala, ma kota.";
    txt::words all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.pos());
    }
    println();
}
```

Output:

```text
0 5 8 
```

## See also

- [size](size.md): the bytes the element takes
- [sgcl::txt::words::iterator](../words-iterator.md)
