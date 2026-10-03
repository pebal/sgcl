[sgcl](../README.md) › [core](README.md) › [collector](collector/README.md) › [referrer](collector-referrer.md)

# sgcl::collector::referrer::kind

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        struct referrer {
            enum class kind : int { object, buffer, stack, cell, unique, weak };
        };
    };
}
```

`sgcl::collector::referrer::kind` is where a word that points at an object is: what the other fields of a
[referrer](collector-referrer.md) mean depends on it.

| Value | Description |
|---|---|
| `object` | a member of a managed object: `holder` the object, `type` its type, `offset` the word's |
| `buffer` | an element of a container's buffer: `holder` the buffer, `type` the element type as `typeid(T[])`, `offset` from the buffer's start, header included |
| `stack` | a word on a thread's stack: `holder` the word's address, `thread` the thread's id, `type` null |
| `cell` | a cell of a `root_ptr` in unmanaged memory: `holder` the block of cells, `offset` the cell's, `type` null; the `root_ptr` that owns the cell is not known to the collector |
| `unique` | the object a `unique_ptr` owns, a root: `holder` the object itself, `type` its type; in a chain, also the block of cells of a `cell` link, a root by its state, with `type` null |
| `weak` | a cell of a `weak_ptr`, which holds nothing: `holder` the cell, `type` null; listed by `get_referrers`, never in a chain |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    unique_ptr owned = make_tracked<int>(1);
    auto [guard, path] = collector::get_path_to_root(owned.get());
    println("{}", path.size() == 1 && path[0].from == collector::referrer::kind::unique);
}
```

Output:

```text
true
```

## See also

- [referrer](collector-referrer.md): the fields
- [sgcl::collector](collector/README.md)
