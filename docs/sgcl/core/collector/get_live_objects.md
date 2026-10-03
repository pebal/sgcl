[sgcl](../../README.md) › [core](../README.md) › [collector](README.md)

# sgcl::collector::get_live_objects

```cpp
static std::tuple<pause_guard, std::vector<void*>> get_live_objects() noexcept;
```

Runs a full collection, waits for it and returns the addresses of the objects it marked, together with a
`pause_guard`. The addresses are raw pointers: the address of each managed object (what `make_tracked` returned for
it) and, for a container's buffer, the start of its slot, which is the buffer's header rather than its first
element. Zeroes the unused stack below the caller's frame first.

While the guard lives, the collector is paused: it runs no cycle, so the raw pointers in the list stay valid. It is
destroyed at the end of the scope that holds it (or by `reset()`), and the collector resumes. Keep it as short as
the analysis needs; nothing a mutator does waits for it.

## Parameters

None.

## Return value

The guard of the pause and the addresses of the live objects, in a `std::tuple`.

## Complexity

A full cycle, then linear in the number of live objects.

## Exceptions

None.

## Notes

The list is a `std::vector` of raw pointers, not a container of the library: it is made while the collector is
paused, when an allocation on the managed heap could wait for the very cycle the pause holds back, and the pointers
must not become objects the next cycle traces. One guard at a time: a call made while a guard lives waits for a
cycle the paused collector cannot run. The function is declared
always-inline, so that no frame of its own lies between the caller and the stack it zeroes.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

struct Node {
    tracked_ptr<Node> next;
};

int main() {
    tracked_ptr first = make_tracked<Node>();
    first->next = make_tracked<Node>();
    {
        auto [guard, objects] = collector::get_live_objects();
        // the collector is paused: every void* in objects is a live object
        auto live = [&](const void* p) {
            return std::find(objects.begin(), objects.end(), p) != objects.end();
        };
        println("{} {}", live(first.get()), live(first->next.get()));
    }  // the guard is destroyed here, the collector resumes
}
```

Output:

```text
true true
```

## See also

- [get_live_object_count](get_live_object_count.md): the count alone, without a pause
- [get_referrers](get_referrers.md): what holds one of them
- [sgcl::collector](README.md)
