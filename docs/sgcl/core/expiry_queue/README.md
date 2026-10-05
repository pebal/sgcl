[sgcl](../../README.md) › [core](../README.md)

# sgcl::expiry_queue\<T\>

```cpp
#include "sgcl/core/expiry_queue.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class expiry_queue;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

An `expiry_queue<T>` decides what to do with an object once nothing else reaches it, by an observer rather than by
the object's destructor. [watch(object, f)](watch.md) makes a weak cell for the object and keeps `f`
next to it. When a cycle finds the object unreachable it does not destroy it: it keeps it alive for the queue, and
[drain()](drain.md) calls `f` with the object as a `tracked_ptr`, alive one last time, on the thread
that calls `drain()`, at that moment, with the heap in a consistent state. `f` may read the object, release what it
owns (a GPU handle, a file, a cache entry, a registry line), or keep the pointer, which is the object's return to
life: it can be watched again. Then the entry is dropped and the object dies with the next cycle that finds it
unreachable, its destructor as ever.

The difference from a destructor: a destructor runs on the collector's threads under
[the rules of destructors](../README.md#the-rules) (rule 5: no peer through anything but `if_alive()`), and cannot keep
its object. `f` runs on a thread of the program's choosing, sees the whole object with its `tracked_ptr` members
valid, and may resurrect it. The difference from Java's `Cleaner` and Go's `AddCleanup`: those run the cleanup on a
thread of the runtime's and never show the object; here the program says where and when, and gets the object.
Until `drain()` the object stays alive and its `weak_ptr`s lock it: an object found unreachable waits for that
call. The queue drains by itself every so many `watch()` calls, as many as it has entries (a pass costs less than
the calls that paid for it, at least 16); a thread that watches little and wants its cleanups on time calls
`drain()` in its loop.

## Rules

- Its entries are a `sgcl::vector` of a `tracked_ptr` to the cell and the function.
- `f` is an [sgcl::function](../function/README.md): its closure may capture tracked pointers, kept in a managed object of its
  own and followed by the collector. A closure holding a strong pointer to the watched object itself keeps the
  object alive, and the entry never expires: the object comes as the argument instead. Capture a raw pointer to an
  owner that outlives the queue, a reference, or plain data; the object itself comes as the argument.
- `f` runs on the thread that calls `drain()`, at that moment. No collector thread, none of the rules of
  destructors: it may read the object through its `tracked_ptr` members, allocate, copy the pointer into a managed
  object; an object kept that way can be watched again after the drain.
- One queue is used by one thread at a time; threads share it with the program's own synchronization (rule 6). The
  collector's side (finding the object unreachable, keeping it) needs none.
  [entry::cancel](../expiry_queue-entry/cancel.md) alone is an atomic flag and may come from any thread.
- Movable, not copyable. The destructor drops every entry without calling its function: the objects are no longer
  kept and die with the next cycle that finds them unreachable.
- The cost for a program without such queues is a test of an empty list per cycle; with them, a pass over the cells
  per convergence of the marking and one more round of marking for the objects kept, plus their memory until the
  drain.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the watched objects: any type a `tracked_ptr` points to. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `tracked_ptr<T>`: what `watch()` takes and the function gets |
| `weak_type` | `weak_ptr<T>`: what [entry::weak](../expiry_queue-entry/weak.md) returns |
| `function_type` | `function<void(value_type)>`: what an entry keeps; `watch()` converts its callable into it, so the callable takes a `value_type` by value (or anything a `value_type` converts to) and returns nothing |
| `size_type` | `size_t` |
| [entry](../expiry_queue-entry/README.md) | the handle of one entry, what `watch()` returns |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](expiry_queue.md) | constructs an empty queue, or takes the entries of another over |
| `(destructor)` | drops every entry without calling its function, as `clear()` |
| [operator=](operator_assign.md) | drops the entries held, then takes the entries of another over |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the queue holds no entry |
| [size](size.md) | the number of entries not drained yet |

#### Modifiers

| Function | Description |
|---|---|
| [watch](watch.md) | adds an entry for an object, with the function to call when it is found unreachable |
| [drain](drain.md) | calls the functions of the entries whose objects were found unreachable, and drops them |
| [clear](clear.md) | drops every entry without calling its function |

## Complexity

- `watch`: amortized constant: an entry appended, and every so many calls a drain linear in the number of entries,
  after as many calls as the queue had entries.
- `drain`, `clear`: linear in the number of entries, plus the functions `drain` calls.
- `size`, `empty`: constant.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A resource outside the managed heap: released by the queue's function
// on the program's thread, not by the destructor on the collector's.
struct Texture {
    explicit Texture(int id) : id(id) {}
    int id;
};

static void release_texture(int id) {
    println("texture {} released", id);
}

// The pointers juggled here stay in a frame of their own: the stack is
// scanned conservatively, and a stale word in main's frame would keep an
// object alive (README, "Stack roots").
static void use_textures(expiry_queue<Texture>& gone, tracked_ptr<Texture>& kept) {
    for (int id : range(1, 4)) {
        tracked_ptr texture = make_tracked<Texture>(id);
        gone.watch(texture, [](tracked_ptr<Texture> t) { release_texture(t->id); });
        if (id == 2) {
            kept = texture;  // the program keeps this one
        }
    }
}  // textures 1 and 3 are unreachable now; the queue keeps them for drain()

int main() {
    expiry_queue<Texture> gone;  // lives where a tracked_ptr may: here on the stack
    tracked_ptr<Texture> kept;
    use_textures(gone, kept);
    println("{} textures watched", gone.size());

    // optional, for the demonstration only: the collector runs its cycles by itself
    collector::force_collect(true);
    println("{} released by the first drain", gone.drain());
    println("{} still watched: texture {}", gone.size(), kept->id);

    // the function may keep the object: its return to life
    tracked_ptr<Texture> revived;
    gone.watch(kept, [&revived](tracked_ptr<Texture> t) { revived = t; });
    kept = nullptr;
    collector::force_collect(true);  // optional, as above
    gone.drain();  // texture 2's first entry releases it, the second revives it
    println("texture {} is back", revived->id);
    return 0;
}
```

Sample output:

```text
3 textures watched
texture 1 released
texture 3 released
2 released by the first drain
1 still watched: texture 2
texture 2 released
texture 2 is back
```

The second `drain()` calls two functions for texture 2, one per entry: the release, and the one that keeps the
pointer. The object is alive during both, and after the drain it lives on in `revived`; the next cycle that finds it
unreachable, once `revived` is gone, destroys it like any other object.

## See also

- [entry](../expiry_queue-entry/README.md): the handle `watch()` returns, to cancel an entry or reach the object weakly
- [weak_ptr](../weak_ptr/README.md): what `entry::weak()` returns; [tracked_ptr](../tracked_ptr/README.md): what the function gets
- [weak_map](../weak_map/README.md), [weak_set](../weak_set/README.md): for what needs no function, the metadata or the registry that
  forgets an object by itself
- [function](../function/README.md): what an entry keeps
- [collector](../collector/README.md): `force_collect(true)`, used above to make the cycle happen at once
- README: [The rules](../README.md#the-rules), [Stack roots](../../../garbage_collector/overview.md#stack-roots)
- `tests/containers/expiry_queue.cpp`: every behaviour above, checked
