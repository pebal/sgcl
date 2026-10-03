[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](README.md)

# sgcl::expiry_queue\<T\>::drain

```cpp
size_type drain();
```

Calls the function of every entry whose object a cycle has found unreachable since the entry was made, with the
object as a `value_type`, and drops the entry. The entries whose objects are still reachable stay; a cancelled
entry is dropped without a call. The order of the calls is not the order of the `watch()` calls. Resets the
automatic drain's count.

The functions run on the thread that calls `drain()`, at that moment, with the heap in a consistent state: they may
read the object through its `tracked_ptr` members, allocate, copy the pointer into a managed object. An object
whose function keeps the pointer lives on, and is found unreachable again by a later cycle only if it is watched
again; an object whose function lets the pointer go dies with the next cycle that finds it unreachable, and the
`weak_ptr`s to it expire then.

## Parameters

None.

## Return value

The number of functions called.

## Complexity

Linear in the number of entries, plus the functions called.

## Exceptions

What a function throws; no entry has an empty function, which [watch()](watch.md) does not queue.

The entry whose function threw has left the queue, and its object is no longer kept; the entries drained before it
are gone, the ones the drain had not reached stay for a later call, and the automatic drain's count is not reset.

## Notes

A cancel from another thread decides against the drain by one atomic flag: a
[cancel()](../expiry_queue-entry/cancel.md) that sets it first, even between the drain's look at the entry and its
call, returns `true`, and the function is not called.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

// Run on a thread of its own: no word of the objects stays on main's stack
static void watch_three(expiry_queue<Texture>& gone, vector<int>& released,
                        tracked_ptr<Texture>& kept) {
    for (int id : range(1, 4)) {
        tracked_ptr texture = make_tracked<Texture>(id);
        gone.watch(texture, [&released](tracked_ptr<Texture> t) { released.push_back(t->id); });
        if (id == 2) {
            kept = texture;
        }
    }
}

int main() {
    expiry_queue<Texture> gone;
    vector<int> released;
    tracked_ptr<Texture> kept;
    thread([&] { watch_three(gone, released, kept); }).join();
    println("{}", gone.drain());  // no cycle has found anything yet

    collector::force_collect(true);  // optional, for the demonstration
    size_t count = gone.drain();
    released.sort();  // the order of the calls is not the order of watch()
    println("{} {} {}", count, released, gone.size());
}
```

Output:

```text
0
2 [1, 3] 1
```

## See also

- [watch](watch.md): adds an entry
- [clear](clear.md): drops every entry without calling its function
- [entry::cancel](../expiry_queue-entry/cancel.md): withdraws one entry
- [sgcl::expiry_queue\<T\>](README.md)
