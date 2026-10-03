[sgcl](../../README.md) › [txt](../README.md) › [sentences](../sentences.md)

# sgcl::txt::sentences::text

```cpp
const slice<const char>& text() const noexcept;
```

Returns the slice of the text the range walks: its bytes, and the object they lie in, which the slice holds. A
position an iterator gives by `pos()` is a position in this slice, and every element is a slice of it.

## Parameters

None.

## Return value

A reference to the slice of the text, valid while the range lives.

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
    txt::sentences all("One. Two.");
    println("{} bytes", all.text().size());
}
```

Output:

```text
9 bytes
```

## See also

- [(constructor)](sentences.md): the range over a text
- [sgcl::txt::sentences](../sentences.md)
