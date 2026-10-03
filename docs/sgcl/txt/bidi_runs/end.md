[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](../bidi_runs.md)

# sgcl::txt::bidi_runs::end

```cpp
auto end() const noexcept;
```

Returns the iterator past the rightmost piece, the last to be drawn.

## Parameters

None.

## Return value

The iterator past the last piece.

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
    txt::bidi_runs pieces("Nazwa: שלום 123 OK");
    println("[{}]", (pieces.end() - 1)->text);
}
```

Output:

```text
[ OK]
```

## See also

- [begin](begin.md): an iterator to the first piece
- [sgcl::txt::bidi_runs](../bidi_runs.md)
