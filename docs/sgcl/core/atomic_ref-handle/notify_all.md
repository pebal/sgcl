[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::notify_all

```cpp
void notify_all() noexcept;
```

Wakes every thread blocked in [wait](wait.md) on the handle's word, as `std::atomic_ref::notify_all` does. It is
called after the store the waiting threads wait for.

## Parameters

None.

## Return value

None.

## Complexity

What the platform's wake of the waiting threads costs.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Race {
    string signal;
    atomic<int> started = 0;
};

int main() {
    tracked_ptr race = make_tracked<Race>();
    string none = atomic_ref(race->signal).load();
    vector<thread> runners;
    for (int i : range(3)) {
        runners.emplace_back([race, none] {
            atomic_ref(race->signal).wait(none);
            ++race->started;
        });
    }
    atomic_ref signal(race->signal);
    signal = string("go");
    signal.notify_all();
    for (auto& r : runners) {
        r.join();
    }
    println("{} started on \"{}\"", race->started.load(), signal.load());
}
```

Output:

```text
3 started on "go"
```

## See also

- [notify_one](notify_one.md): wakes one waiting thread
- [wait](wait.md): blocks while the handle holds the object given
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
