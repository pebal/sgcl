[sgcl](../../README.md) › [txt](../README.md) › [patch_error](README.md)

# sgcl::txt::patch_error::line

```cpp
size_t line() const noexcept;
```

Returns the line of the patch, from 1, that the error is about: the header of the hunk that stands nowhere or is
shorter than it says, the line without a mark; 0 when the patch has no hunk.

## Parameters

None.

## Return value

The line.

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
    println("{}", txt::apply_patch("a\n", "--- a\n+++ b\n@@ -1 +1 @@\n*a\n").error().line());
}
```

Output:

```text
4
```

## See also

- [hunk](hunk.md)
- [sgcl::txt::patch_error](README.md)
