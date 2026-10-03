[sgcl](../../README.md) › [txt](../README.md) › [line_breaks](README.md)

# sgcl::txt::line_breaks::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../line_breaks-iterator/README.md) past the last element: at the byte position of the end of the text,
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
    string s = "jeden dwa trzy";
    txt::line_breaks pieces(s);
    for (auto it = pieces.begin(); it != pieces.end(); ++it) {
        print("[{}]", *it);
    }
    println();
}
```

Output:

```text
[jeden ][dwa ][trzy]
```

## See also

- [begin](begin.md): an iterator to the first element
- [sgcl::txt::line_breaks](README.md)
