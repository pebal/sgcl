[sgcl](../../README.md) › [txt](../README.md) › [folded_text](../folded_text.md)

# sgcl::txt::folded_text::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text mapped to no code points, `size() == 0`: only an empty text does.

## Parameters

None.

## Return value

`true` when the text mapped to nothing, `false` otherwise.

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
    txt::folded_text none;
    txt::folded_text some("a");
    println("{} {} {}", none.empty(), some.empty(), some.find("")->pos == 0);
}
```

Output:

```text
true false true
```

## See also

- [size](size.md): the number of code points
- [sgcl::txt::folded_text, normalized_text](../folded_text.md)
