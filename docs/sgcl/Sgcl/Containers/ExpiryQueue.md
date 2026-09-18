# Sgcl::ExpiryQueue

```cpp
#include "sgcl/Sgcl/Containers/ExpiryQueue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class ExpiryQueue;
}
```

The same class in the `sgcl` interface: [expiry_queue](../../containers/expiry_queue.md).

An `ExpiryQueue<T>` decides what to do with an object once nothing else reaches it, by an observer rather than by the object's destructor. `Watch(object, f)` makes a weak cell for the object and keeps `f` next to it. When a cycle finds the object unreachable it does not destroy it: it keeps it alive for the queue, and `Drain()` calls `f` with the object as a `Ptr`, alive one last time, on the thread that calls `Drain()`, at that moment, with the heap in a consistent state. `f` may read the object, release what it owns (a GPU handle, a file, a cache entry, a registry line), or keep the pointer, which is the object's return to life: it can be watched again. Then the entry is dropped and the object dies with the next cycle that finds it unreachable, its destructor as ever.

The difference from a destructor: a destructor runs on the collector's threads under [the rules of destructors](../../core/README.md#the-rules) (rule 5: no peer through anything but `IfAlive()`), and cannot keep its object. `f` runs on a thread of the program's choosing, sees the whole object with its `Ptr` members valid, and may resurrect it. The difference from Java's `Cleaner` and Go's `AddCleanup`: those run the cleanup on a thread of the runtime's and never show the object; here the program says where and when, and gets the object. Until `Drain()` the object stays alive and its `WeakPtr`s lock it: an object found unreachable waits for that call. The queue drains by itself every so many `Watch()` calls, as many as it has entries (a pass costs less than the calls that paid for it, at least 16); a thread that watches little and wants its cleanups on time calls `Drain()` in its loop.

## Rules

- An `ExpiryQueue` lives where a `Ptr` may: on a stack or inside a managed object; never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), rule 1). Its entries are a managed list of a `Ptr` to the cell and the function.
- `f` is a [`Function`](../Core/Function.md): its closure may capture tracked pointers, kept in a managed object of its own and followed by the collector. A closure holding a strong pointer to the watched object itself keeps the object alive, and the entry never expires: the object comes as the argument instead. Capture a raw pointer to an owner that outlives the queue, a reference, or plain data; the object itself comes as the argument.
- `f` runs on the thread that calls `Drain()`, at that moment. No collector thread, none of the rules of destructors: it may read the object through its `Ptr` members, allocate, copy the pointer into a managed object; an object kept that way can be watched again after the drain.
- One queue is used by one thread at a time; threads share it with the program's own synchronization (rule 6). The collector's side (finding the object unreachable, keeping it) needs none.
- Movable, not copyable. The destructor drops every entry without calling its function: the objects are no longer kept and die with the next cycle that finds them unreachable.
- The cost for a program without such queues is a test of an empty list per cycle; with them, a pass over the cells per convergence of the marking and one more round of marking for the objects kept, plus their memory until the drain.

## Members

### Types

```cpp
using ValueType = Ptr<T>;
using InnerType = sgcl::expiry_queue<T>;
using SizeType = size_t;
class Entry;                                        // the handle Watch() returns
```

`ValueType` is the queue's pointer, `Ptr<T>`, what the function gets; `Entry` the handle `Watch()` returns. `Watch()` converts its callable into a `Function<void(Ptr<T>)>`, so the callable takes a `Ptr<T>` by value (or anything a `Ptr<T>` converts to) and returns nothing.

### Constructors, assignment, destructor

```cpp
ExpiryQueue();
ExpiryQueue(ExpiryQueue&&) noexcept;
ExpiryQueue& operator=(ExpiryQueue&&) noexcept;
ExpiryQueue(const ExpiryQueue&) = delete;
ExpiryQueue& operator=(const ExpiryQueue&) = delete;
~ExpiryQueue();   // Clear()
```

An empty queue costs nothing beyond an empty list. A move hands the entries over; the moved-from queue is empty. The destructor is `Clear()`: the entries are dropped without a call, the objects they kept are no longer kept.

```cpp
struct Entry {};
struct Cache {
    ExpiryQueue<Entry> evicted;      // inside a managed object: allowed
};
Ptr cache = Make<Cache>();
ExpiryQueue<Entry> local;            // on a stack: allowed
ExpiryQueue<Entry> moved = std::move(local);
```

### Watch

```cpp
template<class F>
Entry Watch(const Ptr<T>& object, F&& onExpire);
```

Adds an entry for `object`: a fresh weak cell marked as watched, with `onExpire` stored as a `Function`, and returns the entry's handle (below), which the caller may keep or discard. A null `object` gets no entry and an empty handle is returned. Once every so many calls (as many as the queue had entries after its last drain, at least 16) the call runs `Drain()` itself, so `onExpire` functions of earlier entries may run inside `Watch()`. An object may be watched by several entries or several queues; the cycle that finds it unreachable marks every one of them expired, and each function gets the object.

```cpp
struct Texture { int id; };
auto Release = [](int) {};
ExpiryQueue<Texture> gone;
Ptr texture = Make<Texture>(7);
auto entry = gone.Watch(texture, [&](Ptr<Texture> t) {
    Release(t->id);                      // the object, with its data, one last time
});
assert(entry.Weak().Lock() == texture);  // the ordinary weak pointer to it
```

### Entry

```cpp
class Entry {
public:
    Entry() noexcept;
    bool Cancel() noexcept;
    bool IsExpired() const noexcept;
    WeakPtr<T> Weak() const noexcept;
    explicit operator bool() const noexcept;
};
```

The handle of one entry, sharing the entry's cell; copies share the entry, and it lives where the queue's pointers live. `Cancel()` withdraws the entry: the object is no longer kept for the queue, its function will not be called, and the entry leaves the queue with the next `Drain()` (until then `Count()` counts it); true when the entry was still pending, false for an entry drained or cancelled before, or an empty handle. What `Drain()` does to an entry after calling its function, and `Clear()` to every entry, `Cancel()` does to one without the call: for a resource the program released by hand, or an object another owner took over. From any thread, one atomic flag. `IsExpired()` is true once a cycle has found the object unreachable (its function waits for `Drain()`, or ran, or the entry was cancelled after). `Weak()` is an ordinary `WeakPtr` to the object, sharing the cell, holding nothing; dropping it, or the handle, cancels nothing.

```cpp
struct Texture { int id; };
auto Release = [](int) {};
ExpiryQueue<Texture> gone;
Ptr texture = Make<Texture>(7);
auto entry = gone.Watch(texture, [&](Ptr<Texture> t) { Release(t->id); });
// ... the program releases the texture itself
Release(texture->id);
entry.Cancel();                          // no second release when nothing reaches the texture
```

### Drain

```cpp
SizeType Drain();
```

Calls the function of every entry whose object a cycle has found unreachable since the entry was made, with the object as a `Ptr<T>`, and drops the entry; returns how many. The entries whose objects are still reachable stay. The order of the calls is not the order of the `Watch()` calls. An object whose function keeps the pointer lives on, and is found unreachable again by a later cycle only if it is watched again; an object whose function lets the pointer go dies with the next cycle that finds it unreachable, and the `WeakPtr`s to it expire then. Resets the automatic drain's count.

```cpp
struct Texture { int id; };
ExpiryQueue<Texture> gone;
// in the frame loop: the cleanups on this thread, at this point
size_t released = gone.Drain();
if (released) {
    std::cout << "released " << released << " textures\n";
}
```

### Count, IsEmpty

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
```

The entries not drained yet, whether their objects have been found unreachable or not. An entry leaves the queue only through `Drain()` (when its object was found unreachable) or `Clear()`.

```cpp
struct Texture { int id; };
ExpiryQueue<Texture> gone;
auto f = [](Ptr<Texture>) {};
Ptr a = Make<Texture>(1), b = Make<Texture>(2);
gone.Watch(a, f);
gone.Watch(b, f);
assert(gone.Count() == 2 && !gone.IsEmpty());   // both alive: both still in the queue
```

### Clear

```cpp
void Clear() noexcept;
```

Drops every entry without calling its function: the objects are no longer kept for the queue, and one already found unreachable dies with the next cycle that finds it so (its `WeakPtr`s expire then). Resets the automatic drain's count. The destructor calls it.

```cpp
struct Texture { int id; };
ExpiryQueue<Texture> gone;
gone.Clear();                                  // no function runs; what was kept is let go
assert(gone.IsEmpty());
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The queue inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A resource outside the managed heap: released by the queue's function
// on the program's thread, not by the destructor on the collector's.
struct Texture {
    explicit Texture(int id) : id(id) {}
    int id;
};

static void ReleaseTexture(int id) {
    std::cout << "texture " << id << " released\n";
}

// The pointers juggled here stay in a frame of their own: the stack is
// scanned conservatively, and a stale word in main's frame would keep an
// object alive (README, "Stack roots").
static void UseTextures(ExpiryQueue<Texture>& gone, Ptr<Texture>& kept) {
    for (int id : Range(1, 4)) {
        Ptr texture = Make<Texture>(id);
        gone.Watch(texture, [](Ptr<Texture> t) { ReleaseTexture(t->id); });
        if (id == 2) {
            kept = texture;   // the program keeps this one
        }
    }
}   // textures 1 and 3 are unreachable now; the queue keeps them for Drain()

int main() {
    ExpiryQueue<Texture> gone;          // lives where a Ptr may: here on the stack
    Ptr<Texture> kept;
    UseTextures(gone, kept);
    std::cout << gone.Count() << " textures watched\n";

    Collector::Collect(true);           // optional, for the demonstration only: the collector runs its cycles by itself
    std::cout << gone.Drain() << " released by the first drain\n";   // 1 and 3, in either order
    std::cout << gone.Count() << " still watched: texture " << kept->id << "\n";

    // the function may keep the object: its return to life
    Ptr<Texture> revived;
    gone.Watch(kept, [&revived](Ptr<Texture> t) { revived = t; });
    kept = nullptr;
    Collector::Collect(true);           // optional, as above
    gone.Drain();                                // texture 2's first entry releases it, the second revives it
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

The second `Drain()` calls two functions for texture 2, one per entry: the release, and the one that keeps the pointer. The object is alive during both, and after the drain it lives on in `revived`; the next cycle that finds it unreachable, once `revived` is gone, destroys it like any other object.

## See also

- [WeakPtr](../Core/WeakPtr.md): what `Entry::Weak()` returns; [Ptr](../Core/Ptr.md): what the function gets.
- [WeakDictionary](WeakDictionary.md), [WeakHashSet](WeakHashSet.md): for what needs no function, the metadata or the registry that forgets an object by itself.
- [Collector](../Core/Collector.md): `Collect(true)`, used above to make the cycle happen at once.
- README: [expiry_queue](../../core/README.md#expiry_queue) under [Weak pointers](../../core/README.md#weak-pointers), [The rules](../../core/README.md#the-rules), [Stack roots](../../../garbage_collector/overview.md#stack-roots).
