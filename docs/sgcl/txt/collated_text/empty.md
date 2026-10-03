[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text weighed to no elements, `size() == 0`: only an empty text does.

## Parameters

None.

## Return value

`true` when the text weighed to nothing, `false` otherwise.

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
    txt::collated_text none;
    txt::collated_text some(txt::collator(), "a");
    println("{} {}", none.empty(), some.empty());
}
```

Output:

```text
true false
```

## See also

- [size](size.md): the number of elements
- [sgcl::txt::collated_text](../collated_text.md)
