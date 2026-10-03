[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](README.md)

# sgcl::expiry_queue\<T\>::watch

```cpp
template<class F>
entry watch(const value_type& object, F&& on_expire);
```

Adds an entry for `object`: a fresh weak cell marked as watched, with `on_expire` stored as a `function_type`, and
returns the entry's handle, which the caller may keep or discard. A pointer of another kind that converts to
`value_type` (a `root_ptr<T>`, a `tracked_ptr` to a derived class) converts on the way in. A null `object` gets no
entry, and an empty handle is returned; so does an `on_expire` that is empty (a null function pointer, an empty
`function` or `std::function`), which [drain()](drain.md) would have nothing to call.

When a cycle finds the object unreachable, it keeps the object alive for the queue, and the next
[drain()](drain.md) calls `on_expire` with it. An object may be watched by several entries or several queues; the
cycle that finds it unreachable marks every one of them expired, and each function gets the object.

Once every so many calls (as many as the queue had entries after its last drain, at least 16) the call runs
`drain()` itself, after adding its entry, so `on_expire` functions of earlier entries may run inside `watch()`.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to watch |
| `on_expire` | what to call with the object once it is found unreachable: a callable that takes a `value_type` (or anything a `value_type` converts to) and returns nothing, copyable, converted into a `function_type` |

## Return value

The [entry](../expiry_queue-entry/README.md) of the object, a handle that shares the entry's cell; an empty handle for a
null `object` or an empty `on_expire`.

## Complexity

Amortized constant: the entry is appended; every so many calls, a drain linear in the number of entries, after as
many calls as the queue had entries.

## Exceptions

- What the construction of the `function_type` throws, the copy or the move of `on_expire`. The function is made
  before the cell, so the queue is then as it was.
- When the call drains the queue, what [drain](drain.md) throws: the new entry is in the queue then, and the handle
  is not returned.

## Notes

`on_expire` is an [sgcl::function](../function/README.md): its closure may capture tracked pointers, followed by the
collector. A closure that holds a strong pointer to the watched object itself keeps the object alive, and the entry
never expires: the object comes as the argument instead.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

static int released = 0;

// Objects nothing else holds: they die when the frame does
static void watch_dropped(expiry_queue<Texture>& gone, int count) {
    for (int id : range(count)) {
        tracked_ptr texture = make_tracked<Texture>(id);
        gone.watch(texture, [](tracked_ptr<Texture>) { ++released; });
    }
}

int main() {
    expiry_queue<Texture> gone;
    tracked_ptr kept = make_tracked<Texture>(100);
    auto entry = gone.watch(kept, [](tracked_ptr<Texture>) { ++released; });
    println("{} {}", bool(entry), entry.weak().lock() == kept);

    auto none = gone.watch(nullptr, [](tracked_ptr<Texture>) { ++released; });
    println("{} {}", bool(none), gone.size());

    // on a thread of its own, so that no word of the objects stays on main's stack
    thread([&] { watch_dropped(gone, 15); }).join();  // 16 entries made, no drain yet
    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", gone.size(), released);

    watch_dropped(gone, 1);  // the 17th call drains the queue
    println("{} {}", gone.size(), released);
}
```

Output:

```text
true true
false 1
16 0
2 15
```

## See also

- [drain](drain.md): calls the functions of the entries whose objects were found unreachable
- [entry](../expiry_queue-entry/README.md): the handle `watch` returns
- [sgcl::expiry_queue\<T\>](README.md)
