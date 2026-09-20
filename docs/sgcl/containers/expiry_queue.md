# sgcl::expiry_queue

```cpp
#include "sgcl/sgcl.h"        // or "sgcl/containers/expiry_queue.h"

namespace sgcl {
    template<class T>
    class expiry_queue;
}
```

An `expiry_queue<T>` decides what to do with an object once nothing else reaches it, by an observer rather than by the object's destructor. `watch(object, f)` makes a weak cell for the object and keeps `f` next to it. When a cycle finds the object unreachable it does not destroy it: it keeps it alive for the queue, and `drain()` calls `f` with the object as a `tracked_ptr`, alive one last time, on the thread that calls `drain()`, at that moment, with the heap in a consistent state. `f` may read the object, release what it owns (a GPU handle, a file, a cache entry, a registry line), or keep the pointer, which is the object's return to life: it can be watched again. Then the entry is dropped and the object dies with the next cycle that finds it unreachable, its destructor as ever.

The difference from a destructor: a destructor runs on the collector's threads under [the rules of destructors](../core/README.md#the-rules) (rule 5: no peer through anything but `if_alive()`), and cannot keep its object. `f` runs on a thread of the program's choosing, sees the whole object with its `tracked_ptr` members valid, and may resurrect it. The difference from Java's `Cleaner` and Go's `AddCleanup`: those run the cleanup on a thread of the runtime's and never show the object; here the program says where and when, and gets the object. Until `drain()` the object stays alive and its `weak_ptr`s lock it: an object found unreachable waits for that call. The queue drains by itself every so many `watch()` calls, as many as it has entries (a pass costs less than the calls that paid for it, at least 16); a thread that watches little and wants its cleanups on time calls `drain()` in its loop.

## Rules

- An `expiry_queue` lives where a `tracked_ptr` may: on a stack or inside a managed object; never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../core/README.md#the-rules), rule 1). Its entries are a `sgcl::vector` of a `tracked_ptr` to the cell and the function.
- `f` is an [`sgcl::function`](../core/function.md): its closure may capture tracked pointers, kept in a managed object of its own and followed by the collector. A closure holding a strong pointer to the watched object itself keeps the object alive, and the entry never expires: the object comes as the argument instead. Capture a raw pointer to an owner that outlives the queue, a reference, or plain data; the object itself comes as the argument.
- `f` runs on the thread that calls `drain()`, at that moment. No collector thread, none of the rules of destructors: it may read the object through its `tracked_ptr` members, allocate, copy the pointer into a managed object; an object kept that way can be watched again after the drain.
- One queue is used by one thread at a time; threads share it with the program's own synchronization (rule 6). The collector's side (finding the object unreachable, keeping it) needs none.
- Movable, not copyable. The destructor drops every entry without calling its function: the objects are no longer kept and die with the next cycle that finds them unreachable.
- The cost for a program without such queues is a test of an empty list per cycle; with them, a pass over the cells per convergence of the marking and one more round of marking for the objects kept, plus their memory until the drain.

## Members

### Types

```cpp
using value_type = tracked_ptr<T>;
using weak_type = weak_ptr<T>;
using function_type = function<void(value_type)>;   // sgcl::function of the queue's kind
using size_type = size_t;
```

`value_type` is the queue's pointer, `tracked_ptr<T>`; `weak_type` the weak pointer, what `entry::weak()` returns; `entry` the handle `watch()` returns. `function_type` is what an entry keeps: `watch()` converts its callable into it, so the callable takes a `value_type` by value (or anything a `value_type` converts to) and returns nothing.

### Constructors, assignment, destructor

```cpp
expiry_queue() = default;
expiry_queue(expiry_queue&&) noexcept;
expiry_queue& operator=(expiry_queue&&) noexcept;   // clear(), then the entries of the other
expiry_queue(const expiry_queue&) = delete;
expiry_queue& operator=(const expiry_queue&) = delete;
~expiry_queue();   // clear()
```

An empty queue costs nothing beyond an empty `sgcl::vector`. A move hands the entries over; the moved-from queue is empty. Move assignment first drops the entries this queue held, as `clear()` does: without a call, and the objects they kept are no longer kept, whatever handles or weak pointers of theirs live on. The destructor is `clear()`: the entries are dropped without a call, the objects they kept are no longer kept.

```cpp
struct Cache {
    sgcl::expiry_queue<Entry> evicted;     // inside a managed object: allowed
};
sgcl::tracked_ptr cache = sgcl::make_tracked<Cache>();
sgcl::expiry_queue<Entry> local;           // on a stack: allowed
sgcl::expiry_queue<Entry> moved = std::move(local);
```

### watch

```cpp
template<class F>
entry watch(const value_type& object, F&& on_expire);
```

Adds an entry for `object`: a fresh weak cell marked as watched, with `on_expire` stored as a `function_type`, and returns the entry's handle (below), which the caller may keep or discard; the pointer of the other kind converts to `value_type` on the way in. A null `object` gets no entry and an empty handle is returned. Once every so many calls (as many as the queue had entries after its last drain, at least 16) the call runs `drain()` itself, so `on_expire` functions of earlier entries may run inside `watch()`. An object may be watched by several entries or several queues; the cycle that finds it unreachable marks every one of them expired, and each function gets the object.

```cpp
sgcl::expiry_queue<Texture> gone;
sgcl::tracked_ptr texture = sgcl::make_tracked<Texture>(upload(pixels));
auto entry = gone.watch(texture, [](sgcl::tracked_ptr<Texture> t) {
    release(t->id);                      // the object, with its data, one last time
});
assert(entry.weak().lock() == texture);  // the ordinary weak pointer to it
```

### entry

```cpp
class entry {
public:
    entry() noexcept;
    bool cancel() noexcept;
    bool expired() const noexcept;
    weak_type weak() const noexcept;
    explicit operator bool() const noexcept;
};
```

The handle of one entry, sharing the entry's cell; copies share the entry, and it lives where the queue's pointers live. `cancel()` withdraws the entry: the object is no longer kept for the queue, its function will not be called, and the entry leaves the queue with the next `drain()` (until then `size()` counts it); true when the entry was still pending, false for an entry drained or cancelled before, or an empty handle. The answer is exact even against a `drain()` running on another thread: one atomic flag decides, and a `cancel()` that sets it first, even between the drain's look at the entry and its call, is the one that returns true, and the function is not called. What `drain()` does to an entry after calling its function, and `clear()` to every entry, `cancel()` does to one without the call: for a resource the program released by hand, or an object another owner took over. From any thread, one atomic flag. `expired()` is true once a cycle has found the object unreachable (its function waits for `drain()`, or ran, or the entry was cancelled after). `weak()` is an ordinary `weak_type` to the object, sharing the cell, holding nothing; dropping it, or the handle, cancels nothing.

```cpp
auto entry = gone.watch(texture, [](sgcl::tracked_ptr<Texture> t) { release(t->id); });
// ... the program releases the texture itself
release(texture->id);
entry.cancel();                          // no second release when nothing reaches the texture
```

### drain

```cpp
size_type drain();
```

Calls the function of every entry whose object a cycle has found unreachable since the entry was made, with the object as a `value_type`, and drops the entry; returns how many. The entries whose objects are still reachable stay. The order of the calls is not the order of the `watch()` calls. An object whose function keeps the pointer lives on, and is found unreachable again by a later cycle only if it is watched again; an object whose function lets the pointer go dies with the next cycle that finds it unreachable, and the `weak_ptr`s to it expire then. Resets the automatic drain's count.

```cpp
// in the frame loop: the cleanups on this thread, at this point
size_t released = gone.drain();
if (released) {
    log("released ", released, " textures");
}
```

### size, empty

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
```

The entries not drained yet, whether their objects have been found unreachable or not. An entry leaves the queue only through `drain()` (when its object was found unreachable) or `clear()`.

```cpp
gone.watch(a, f);
gone.watch(b, f);
assert(gone.size() == 2 && !gone.empty());   // both alive: both still in the queue
```

### clear

```cpp
void clear() noexcept;
```

Drops every entry without calling its function: the objects are no longer kept for the queue, and one already found unreachable dies with the next cycle that finds it so (its `weak_ptr`s expire then). Resets the automatic drain's count. The destructor calls it.

```cpp
gone.clear();                                  // no function runs; what was kept is let go
assert(gone.empty());
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A resource outside the managed heap: released by the queue's function
// on the program's thread, not by the destructor on the collector's.
struct Texture {
    explicit Texture(int id) : id(id) {}
    int id;
};

static void release_texture(int id) {
    std::cout << "texture " << id << " released\n";
}

// The pointers juggled here stay in a frame of their own: the stack is
// scanned conservatively, and a stale word in main's frame would keep an
// object alive (README, "Stack roots").
static void use_textures(sgcl::expiry_queue<Texture>& gone, sgcl::tracked_ptr<Texture>& kept) {
    for (int id : sgcl::range(1, 4)) {
        sgcl::tracked_ptr texture = sgcl::make_tracked<Texture>(id);
        gone.watch(texture, [](sgcl::tracked_ptr<Texture> t) { release_texture(t->id); });
        if (id == 2) {
            kept = texture;   // the program keeps this one
        }
    }
}   // textures 1 and 3 are unreachable now; the queue keeps them for drain()

int main() {
    sgcl::expiry_queue<Texture> gone;            // lives where a tracked_ptr may: here on the stack
    sgcl::tracked_ptr<Texture> kept;
    use_textures(gone, kept);
    std::cout << gone.size() << " textures watched\n";

    sgcl::collector::force_collect(true);        // optional, for the demonstration only: the collector runs its cycles by itself
    std::cout << gone.drain() << " released by the first drain\n";   // 1 and 3, in either order
    std::cout << gone.size() << " still watched: texture " << kept->id << "\n";

    // the function may keep the object: its return to life
    sgcl::tracked_ptr<Texture> revived;
    gone.watch(kept, [&revived](sgcl::tracked_ptr<Texture> t) { revived = t; });
    kept = nullptr;
    sgcl::collector::force_collect(true);        // optional, as above
    gone.drain();                              // texture 2's first entry releases it, the second revives it
    std::cout << "texture " << revived->id << " is back\n";
    return 0;
}
```

The output:

```
3 textures watched
texture 1 released
texture 3 released
2 released by the first drain
1 still watched: texture 2
texture 2 released
texture 2 is back
```

The second `drain()` calls two functions for texture 2, one per entry: the release, and the one that keeps the pointer. The object is alive during both, and after the drain it lives on in `revived`; the next cycle that finds it unreachable, once `revived` is gone, destroys it like any other object.

## See also

- [weak_ptr](../core/weak_ptr.md): what `entry::weak()` returns; [tracked_ptr](../core/tracked_ptr.md): what the function gets.
- [weak_map](weak_map.md), [weak_set](weak_set.md): for what needs no function, the metadata or the registry that forgets an object by itself.
- [collector](../core/collector.md): `force_collect(true)`, used above to make the cycle happen at once.
- README: [expiry_queue](../core/README.md#expiry_queue) under [Weak pointers](../core/README.md#weak-pointers), [The rules](../core/README.md#the-rules), [Stack roots](../../garbage_collector/overview.md#stack-roots).
- `tests/containers/expiry_queue.cpp`: every behaviour above, checked.
