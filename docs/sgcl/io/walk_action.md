[sgcl](../README.md) › [io](README.md)

# sgcl::io::walk_action

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class walk_action { next, skip_dir, stop };
}
```

What the function given to [walk_dir](walk_dir.md) returns for an entry: how the walk goes on. Go's `WalkDirFunc`
says it with an error, `nil`, `fs.SkipDir` or `fs.SkipAll`.

| Value | Description |
|---|---|
| `next` | go on: into the entry when it is a directory, else to the next entry |
| `skip_dir` | do not enter this directory; for an entry that is not one, the same as `next` |
| `stop` | end the walk; `walk_dir` returns success |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("tree/a/deep");
    (void)io::mkdir_all("tree/b");
    (void)io::mkdir_all("tree/c");
    (void)io::walk_dir("tree", [](const io::directory_entry& e, const optional<io::error>&) {
        println("{}", e.path);
        if (e.name == "a") {
            return io::walk_action::skip_dir;
        }
        return e.name == "b" ? io::walk_action::stop : io::walk_action::next;
    });
}
```

Output:

```text
tree/a
tree/b
```

## See also

- [walk_dir](walk_dir.md): the walk
