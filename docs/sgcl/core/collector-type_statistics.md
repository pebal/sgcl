[sgcl](../README.md) › [core](README.md) › [collector](collector/README.md)

# sgcl::collector::type_statistics

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        struct type_statistics {
            const std::type_info* type;
            bool buffers;
            size_t object_size;
            size_t live_objects;
            size_t live_bytes;
            size_t pages;
        };
    };
}
```

`sgcl::collector::type_statistics` is the live objects of one type after a full cycle, an element of what
[get_type_statistics](collector/get_type_statistics.md) returns. Objects are counted by their type; the buffers of
the containers by their array type, `typeid(T[])` for elements `T`, with `buffers == true`, the slot they occupy as
their bytes and no pages, since the pages of buffers belong to size classes rather than to a type.

## Member objects

| Field | Description |
|---|---|
| `type` | the objects' type, or the array type of a buffer (`typeid(T[])`) |
| `buffers` | `true` for the buffers of the containers |
| `object_size` | the bytes of one object, or of one element of a buffer: the slot size, at least `sizeof(T)` |
| `live_objects` | the objects (or buffers) of this type after the cycle |
| `live_bytes` | their bytes: the slots they occupy |
| `pages` | the pages of this type's pools; `0` for buffers |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<tracked_ptr<int>> kept;
    for (int i : range(1000)) {
        kept.push_back(make_tracked<int>(i));
    }
    for (const collector::type_statistics& t : collector::get_type_statistics()) {
        print("{}{}: {} x {} B = {} B", (t.buffers ? "buffers of " : ""), t.type->name(),
              t.live_objects, t.object_size, t.live_bytes);
        if (!t.buffers) {
            print(", {} pages", t.pages);
        }
        println();
    }
}
```

Sample output:

```text
buffers of A_N4sgcl11tracked_ptrIiEE: 1 x 8 B = 8192 B
i: 1000 x 4 B = 4000 B, 1 pages
```

## See also

- [get_type_statistics](collector/get_type_statistics.md): what returns it
- [statistics](collector-statistics.md): the counters of the collector's work
- [sgcl::collector](collector/README.md)
