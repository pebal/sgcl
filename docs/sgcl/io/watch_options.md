[sgcl](../README.md) › [io](README.md)

# sgcl::io::watch_options

```cpp
#include "sgcl/io/watch.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct watch_options {
        bool recursive = false;
        duration coalesce = 50 * millisecond;
    };
}
```

`sgcl::io::watch_options` is how [watch](watch.md) watches a path, by field name: `{.recursive = true}`,
`{.coalesce = duration::zero()}`.

## Member objects

| Field | Description |
|---|---|
| `recursive` | a directory's whole tree, not only its entries; `false` by default; nothing for a file |
| `coalesce` | the events of a path within it merged into one: a burst of writes is one event; 50 ms by default, zero for every event as it comes (macOS: FSEvents' latency) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::mkdir_all("project/src");
    async::stop_source stop;
    async::channel<io::watch_event> changes = io::watch("project", {.recursive = true}, stop.token()).value();
    io::write_file("project/src/main.cpp", "int main() {}");
    for (io::watch_event e : changes) {
        if (e.path == "project/src/main.cpp") {
            println("{}", e.path);
            break;
        }
    }
    stop.request_stop();
}
```

Output:

```text
project/src/main.cpp
```

## See also

- [watch](watch.md)
