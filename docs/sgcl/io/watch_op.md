[sgcl](../README.md) › [io](README.md)

# sgcl::io::watch_op

```cpp
#include "sgcl/io/watch.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class watch_op : uint8_t {
        created = 1,
        modified = 2,
        removed = 4,
        renamed = 8,
        attribute = 16,
        overflow = 32,
    };

    constexpr watch_op operator|(watch_op a, watch_op b) noexcept;
    constexpr bool operator&(watch_op a, watch_op b) noexcept;
}
```

`sgcl::io::watch_op` is what happened to a path, in a [watch_event](watch_event.md) of [watch](watch.md): flags, or-ed
when the system merged several operations of a path into one event, tested with `&`.

| Value | Description |
|---|---|
| `created` | the path was made: a file, a directory, a link |
| `modified` | its contents were written |
| `removed` | it was removed |
| `renamed` | it was renamed, either name: a stat tells the new name from the old |
| `attribute` | its permissions, owner, times or extended attributes changed |
| `overflow` | events were lost, the system's queue or the records waiting full: read again what matters |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::watch_op ops = io::watch_op::created | io::watch_op::modified;
    println("{} {}", ops & io::watch_op::modified, ops & io::watch_op::removed);
}
```

Output:

```text
true false
```

## See also

- [watch_event](watch_event.md), [watch](watch.md)
