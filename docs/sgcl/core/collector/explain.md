[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::explain

```cpp
static void explain(const void* p, std::ostream& out);
```

Writes to `out` what holds the object `p` points into and what it retains: the chain of
[get_path_to_root](get_path_to_root.md) as text, one line per link, and the objects and bytes of
[get_retained](get_retained.md); or why there is no chain: the object is held by nothing but the frames of the
call, or `p` is not a live managed object.

A full cycle runs first and the collector is paused for the walk; the pause ends before the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `p` | a pointer to the object or into it |
| `out` | the stream the text is written to |

## Return value

None.

## Complexity

A full cycle, then a breadth-first search and a walk over the live objects.

## Exceptions

What the stream throws when its exceptions are on.

## Notes

The types are named as `std::type_info::name()` names them, mangled on most compilers.

## Example

```cpp
#include "sgcl/core.h"
#include <iostream>

using namespace sgcl;

struct Leaf {
    int value;
};

struct Node {
    tracked_ptr<Node> next;
    tracked_ptr<Leaf> leaf;
};

int main() {
    unique_ptr head = make_tracked<Node>();
    head->next = make_tracked<Node>();
    head->next->leaf = make_tracked<Leaf>(7);
    collector::explain(head->next->leaf.get(), std::cout);

    int local = 0;
    collector::explain(&local, std::cout);
}
```

Sample output:

```text
0x10000260000 is held by
  a 4Node at 0x10000270010, the word at byte 8
  a 4Node at 0x10000270000, the word at byte 0
  a unique_ptr: the 4Node at 0x10000270000 is its object
and keeps alive 1 object, 4 bytes, itself included
0x16f22e648: not a live managed object
```

## See also

- [get_path_to_root](get_path_to_root.md): the chain as values
- [get_retained](get_retained.md): what dies with the object
- [sgcl::collector](../collector.md)
