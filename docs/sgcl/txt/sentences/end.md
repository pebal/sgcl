[sgcl](../../README.md) › [txt](../README.md) › [sentences](../sentences.md)

# sgcl::txt::sentences::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../sentences-iterator.md) past the last element: at the byte position of the end of the text,
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
    string s = "One. Two.";
    txt::sentences all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("[{}]", *it);
    }
    println();
}
```

Output:

```text
[One. ][Two.]
```

## See also

- [begin](begin.md): an iterator to the first element
- [sgcl::txt::sentences](../sentences.md)
