[sgcl](../../README.md) › [txt](../README.md) › [line_breaks](../line_breaks/README.md) › [iterator](README.md)

# sgcl::txt::line_breaks::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position of the iterator's element in the text: where a piece that must stay together begins. The
position of [end](../line_breaks/end.md) is the size of the text.

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
    txt::line_breaks pieces(s);
    for (auto it = pieces.begin(); it != pieces.end(); ++it) {
        print("{} ", it.pos());
    }
    println();
}
```

Output:

```text
0 4 7 
```

## See also

- [size](size.md): the bytes the element takes
- [sgcl::txt::line_breaks::iterator](README.md)
