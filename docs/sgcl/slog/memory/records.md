[sgcl](../../README.md) › [slog](../README.md) › [memory](../memory.md)

# sgcl::slog::memory::records

```cpp
vector<record> records() const noexcept;
```

Returns the records kept so far, in the order they came: a new vector of the clones, which share what they own with
the ones kept. A record logged on another thread while the call runs is in it or not, whole.

## Parameters

None.

## Return value

The records, each a [clone](../record/clone.md) that owns all it holds.

## Complexity

Linear in the number of records kept.

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
    {
        string user = "ala";
        log.info("login", "user", user);
    }
    log.warn("disk almost full", "free", 0.05);
    for (const auto& r : kept.records()) {
        auto a = *r.begin();
        println("{} {} {}={}", r.level() == slog::level::warn, r.message(), a.key(),
                a.value().text());
    }
}
```

Output:

```text
false login user=ala
true disk almost full free=0.05
```

## See also

- [size](size.md), [clear](clear.md)
- [record](../record.md)
- [sgcl::slog::memory](../memory.md)
