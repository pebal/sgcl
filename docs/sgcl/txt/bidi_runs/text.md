[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](README.md)

# sgcl::txt::bidi_runs::text

```cpp
const slice<const char>& text() const noexcept;
```

Returns the slice of the text the pieces are cut from: its bytes, and the object they lie in, which the slice holds.
Every piece is a slice of it.

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
    txt::bidi_runs pieces("Nazwa: שלום");
    println("{} bytes", pieces.text().size());
}
```

Output:

```text
15 bytes
```

## See also

- [(constructor)](bidi_runs.md): the range over a text
- [sgcl::txt::bidi_runs](README.md)
