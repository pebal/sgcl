[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::as_time

```cpp
optional<duration> as_time() const noexcept;
```

The time of the clock of a local time, a local date-time or an offset date-time (in its offset), as a
[duration](../../core/duration/README.md) since midnight, to the nanosecond; `nullopt` for a local date and every
other value.

## Parameters

None.

## Return value

The time since midnight, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = 07:32:00.5\nb = 1979-05-27T07:32:00Z\nc = 1979-05-27").value();
    println("{} {} {}", v["a"].as_time(), v["b"].as_time(), v["c"].as_time());
}
```

Output:

```text
7h32m0.5s 7h32m0s nullopt
```

## See also

- [local_time](local_time.md)
- [sgcl::encoding::toml](README.md)
