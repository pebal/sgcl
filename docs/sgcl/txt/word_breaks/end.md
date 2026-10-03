[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](README.md)

# sgcl::txt::word_breaks::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../word_breaks-iterator/README.md) past the last element: at the byte position of the end of the text,
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
    string s = "a  b";
    txt::word_breaks parts(s);
    for (auto it = parts.begin(); it != parts.end(); ++it) {
        print("[{}]", *it);
    }
    println();
}
```

Output:

```text
[a][  ][b]
```

## See also

- [begin](begin.md): an iterator to the first element
- [sgcl::txt::word_breaks](README.md)
