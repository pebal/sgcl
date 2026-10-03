[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue.md)

# sgcl::expiry_queue\<T\>::clear

```cpp
void clear() noexcept;
```

Drops every entry without calling its function: the objects are no longer kept for the queue, and one already found
unreachable dies with the next cycle that finds it so (its `weak_ptr`s expire then). Resets the automatic drain's
count. The destructor and the move assignment call it.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

What `clear()` does to every entry, [entry::cancel](../expiry_queue-entry/cancel.md) does to one: for resources the
program released by hand, or objects another owner took over.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

// Run on a thread of its own: no word of the object stays on main's stack
static weak_ptr<Texture> watch_dropped(expiry_queue<Texture>& gone, int& calls) {
    tracked_ptr texture = make_tracked<Texture>(1);
    return gone.watch(texture, [&calls](tracked_ptr<Texture>) { ++calls; }).weak();
}

int main() {
    expiry_queue<Texture> gone;
    int calls = 0;
    weak_ptr<Texture> weak;
    thread([&] { weak = watch_dropped(gone, calls); }).join();
    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", gone.size(), weak.expired());  // found unreachable, kept for the queue

    gone.clear();  // no function runs; what was kept is let go
    println("{}", gone.empty());
    collector::force_collect(true);  // optional, as above
    println("{} {}", weak.expired(), calls);
}
```

Output:

```text
1 false
true
true 0
```

## See also

- [drain](drain.md): calls the functions and drops the entries
- [entry::cancel](../expiry_queue-entry/cancel.md): withdraws one entry
- [sgcl::expiry_queue\<T\>](../expiry_queue.md)
