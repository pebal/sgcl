[sgcl](../../README.md) › [txt](../README.md) › [collated_text](README.md)

# sgcl::txt::collated_text::at

```cpp
size_t at(size_t i) const noexcept;
```

Returns where in the text the element `i` came from: the byte where the combining sequence that produced it begins.
Every element of a sequence carries that sequence's position, and the positions ascend, which is what lets a search
from a byte start by a bisection.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the index of the element, below [size](size.md)`()` |

## Return value

The byte position in the text. `i` past the elements is undefined.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collated_text text(txt::collator(), "aé b");
    for (size_t i : range(text.size())) {
        print("{} ", text.at(i));
    }
    println();
}
```

Output:

```text
0 1 1 3 4 
```

## See also

- [size](size.md): the number of elements
- [sgcl::txt::collated_text](README.md)
