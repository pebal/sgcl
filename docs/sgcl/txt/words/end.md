[sgcl](../../README.md) › [txt](../README.md) › [words](../words.md)

# sgcl::txt::words::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../words-iterator.md) past the last element: at the byte position of the end of the text, the
size of the slice.

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
    string s = "Ala ma kota";
    txt::words all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("[{}]", *it);
    }
    println();
}
```

Output:

```text
[Ala][ma][kota]
```

## See also

- [begin](begin.md): an iterator to the first element
- [sgcl::txt::words](../words.md)
