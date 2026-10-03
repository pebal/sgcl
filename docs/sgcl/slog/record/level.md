[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::level

```cpp
slog::level level() const noexcept;
```

Returns the level of the record: the verb's, or the one given to [logger::log](../logger/log.md).

## Parameters

None.

## Return value

The [level](../level.md).

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
    slog::logger log(kept, slog::level::debug);
    log.debug("a");
    log.log(slog::level(6), "b");
    for (const auto& r : kept.records()) {
        println("{} {}", r.message(), int(r.level()));
    }
}
```

Output:

```text
a -4
b 6
```

## See also

- [time](time.md), [message](message.md)
- [sgcl::slog::record](../record.md)
