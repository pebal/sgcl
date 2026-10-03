[sgcl](../../README.md) › [txt](../README.md) › [graphemes](README.md)

# sgcl::txt::graphemes::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../graphemes-iterator/README.md) past the last element: at the byte position of the end of the text,
the size of the slice.

## Parameters

None.

## Return value

The iterator past the last element.

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
    string s = "żółw";
    txt::graphemes letters(s);
    int n = 0;
    for (auto it = letters.begin(); it != letters.end(); ++it) {
        ++n;
    }
    println("{}", n);
}
```

Output:

```text
4
```

## See also

- [begin](begin.md): an iterator to the first element
- [sgcl::txt::graphemes](README.md)
