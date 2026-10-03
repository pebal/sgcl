[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::end

```cpp
attrs::iterator end() const noexcept;
```

Returns the [iterator](../attrs-iterator.md) past the last attribute at the top of the record's tree, which every iterator of the record
reaches at its end. It is not dereferenced.

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
    slog::memory kept;
    slog::logger(kept).info("none");
    auto r = kept.records()[0];
    println("{}", r.begin() == r.end());
}
```

Output:

```text
true
```

## See also

- [begin](begin.md)
- [sgcl::slog::record](../record.md)
