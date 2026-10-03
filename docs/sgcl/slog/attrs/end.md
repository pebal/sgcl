[sgcl](../../README.md) › [slog](../README.md) › [attrs](README.md)

# sgcl::slog::attrs::end

```cpp
iterator end() const noexcept;
```

Returns the [iterator](../attrs-iterator.md) past the last attribute, which every iterator of the range reaches at
its end. It is not dereferenced.

## Parameters

None.

## Return value

The end iterator.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::attrs none;
    println("{}", none.begin() == none.end());
}
```

Output:

```text
true
```

## See also

- [begin](begin.md)
- [sgcl::slog::attrs](README.md)
