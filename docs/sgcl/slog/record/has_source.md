[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::has_source

```cpp
bool has_source() const noexcept;
```

Checks whether the logger was made with [options](../options.md)`::source`, which asks for where the call is to be
written: a handler of the program that writes lines of its own reads it to decide whether to write
[source](source.md).

## Parameters

None.

## Return value

`true` when the logger asks for the source.

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
    slog::logger(kept).info("without");
    slog::logger(slog::options{.handler = kept, .source = true}).info("with");
    for (const auto& r : kept.records()) {
        println("{} {}", r.message(), r.has_source());
    }
}
```

Output:

```text
without false
with true
```

## See also

- [source](source.md)
- [sgcl::slog::record](../record.md)
