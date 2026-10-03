[sgcl](../README.md) › [core](README.md) › [collector](collector/README.md)

# sgcl::collector::referrer

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        struct referrer {
            enum class kind : int { object, buffer, stack, cell, unique, weak };
            kind from;
            const void* holder;
            const std::type_info* type;
            size_t offset;
            std::thread::id thread;
        };
    };
}
```

`sgcl::collector::referrer` is a word that points at an object, or a link of a chain from an object up to a root:
an element of what [get_referrers](collector/get_referrers.md) and [get_path_to_root](collector/get_path_to_root.md)
return. What its fields mean depends on where the word is, `from`.

## Member types

| Type | Definition |
|---|---|
| [kind](collector-referrer-kind.md) | where the word is: in an object, a buffer, a stack, a cell of a `root_ptr`, the object a `unique_ptr` owns, a cell of a `weak_ptr` |

## Member objects

| Field | Description |
|---|---|
| `from` | where the word is, a [kind](collector-referrer-kind.md) |
| `holder` | the object, the buffer or the block that holds the word; the word itself on a stack; for `unique`, the object the `unique_ptr` owns |
| `type` | the holder's type; a buffer's element type as `typeid(T[])`; null for a stack word and for the library's own cells: a `weak_ptr`'s, a `root_ptr`'s and their block |
| `offset` | the word's byte offset in the holder; for a buffer from its start, header included; for a `cell`, the cell's offset in its block |
| `thread` | for a stack word, the thread whose stack it is on |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

int main() {
    vector<tracked_ptr<Node>> nodes;
    nodes.push_back(make_tracked<Node>(1));
    nodes.push_back(make_tracked<Node>(2));
    nodes[0]->next = nodes[1];

    auto [guard, referrers] = collector::get_referrers(nodes[1].get());
    for (const collector::referrer& r : referrers) {
        if (r.from == collector::referrer::kind::object) {
            println("an object, the word at byte {}", r.offset);
        }
    }
    for (const collector::referrer& r : referrers) {
        if (r.from == collector::referrer::kind::buffer) {
            println("a buffer of tracked_ptr<Node>: {}", *r.type == typeid(tracked_ptr<Node>[]));
        }
    }
}
```

Output:

```text
an object, the word at byte 8
a buffer of tracked_ptr<Node>: true
```

## See also

- [get_referrers](collector/get_referrers.md), [get_path_to_root](collector/get_path_to_root.md): what return it
- [sgcl::collector](collector/README.md)
