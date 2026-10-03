[sgcl](../../README.md) › [slog](../README.md) › [record](README.md)

# sgcl::slog::record::empty

```cpp
bool empty() const noexcept;
```

Checks whether the record has no attributes at the top of its tree: `size() == 0`.

## Parameters

None.

## Return value

`true` when the record has no attributes.

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
    slog::logger log(kept);
    log.info("bare");
    log.info("with one", "k", 1);
    for (const auto& r : kept.records()) {
        println("{} {}", r.message(), r.empty());
    }
}
```

Output:

```text
bare true
with one false
```

## See also

- [size](size.md)
- [sgcl::slog::record](README.md)
