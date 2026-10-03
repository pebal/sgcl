[sgcl](../../README.md) › [txt](../README.md) › [collated_text](README.md)

# sgcl::txt::collated_text::bytes

```cpp
const string& bytes() const noexcept;
```

Returns the text the object was built from as the string it holds: the string it was given, or the copy of a piece
or a C text.

## Parameters

None.

## Return value

The string.

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
    string line = "klucz=Wartość";
    txt::collated_text value(txt::collator(), line.as_slice(6));
    println("{} ({} bytes)", value.bytes(), value.bytes().size());
}
```

Output:

```text
Wartość (9 bytes)
```

## See also

- [text](text.md): the same text as a slice
- [sgcl::txt::collated_text](README.md)
