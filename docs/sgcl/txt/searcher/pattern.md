[sgcl](../../README.md) › [txt](../README.md) › [searcher](README.md)

# sgcl::txt::searcher::pattern

```cpp
const string& pattern() const noexcept;
```

Returns the pattern the searcher looks for, as it was given.

## Parameters

None.

## Return value

The pattern.

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
    txt::searcher s("--boundary42");
    println("{} ({} bytes)", s.pattern(), s.pattern().size());
}
```

Output:

```text
--boundary42 (12 bytes)
```

## See also

- [(constructor)](searcher.md): prepares the pattern
- [sgcl::txt::searcher](README.md)
