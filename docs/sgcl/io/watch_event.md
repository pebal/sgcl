[sgcl](../README.md) › [io](README.md)

# sgcl::io::watch_event

```cpp
#include "sgcl/io/watch.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct watch_event {
        string path;
        watch_op ops = watch_op(0);
        bool is_directory = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::watch_event` is one change of the file system as [watch](watch.md) sends it: the path, what happened to it,
whether it is a directory.

## Member objects

| Field | Description |
|---|---|
| `path` | the path watched as it was given (cleaned), joined with the part below it; empty for an overflow |
| `ops` | what happened ([watch_op](watch_op.md)), several or-ed when the system merged them |
| `is_directory` | whether the path is a directory |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::mkdir("tree");
    async::stop_source stop;
    async::channel<io::watch_event> changes = io::watch("tree", {}, stop.token()).value();
    io::mkdir("tree/sub");
    for (io::watch_event e : changes) {
        if (e.path == "tree/sub") {
            println("{} directory {}", e.path, e.is_directory);
            break;
        }
    }
    stop.request_stop();
}
```

Output:

```text
tree/sub directory true
```

## See also

- [watch](watch.md), [watch_op](watch_op.md)
