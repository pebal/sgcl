[sgcl](../README.md) › [core](README.md) › [collector](collector.md)

# sgcl::collector::retained

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        struct retained {
            size_t objects;
            size_t bytes;
        };
    };
}
```

`sgcl::collector::retained` is what dies with an object, what [get_retained](collector/get_retained.md) returns:
the objects reachable from it and from nowhere else, itself included, and their bytes.

## Member objects

| Field | Description |
|---|---|
| `objects` | the objects that die with it, itself included; `0` for a pointer that is not into a live managed object |
| `bytes` | their bytes: the slots they occupy, a container's buffer at the slot of its size class |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Page {
    vector<int> words;
};

int main() {
    tracked_ptr page = make_tracked<Page>();
    page->words.resize(1000);
    collector::retained r = collector::get_retained(page.get());
    println("{} objects, {}", r.objects, r.bytes >= 1000 * sizeof(int));
}
```

Output:

```text
2 objects, true
```

## See also

- [get_retained](collector/get_retained.md): what returns it
- [sgcl::collector](collector.md)
