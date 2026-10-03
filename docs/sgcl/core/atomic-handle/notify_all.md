[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](README.md)

# sgcl::atomic\<H\>::notify_all

```cpp
void notify_all() noexcept;
```

Wakes every thread blocked in [wait](wait.md) on this atomic, as `std::atomic::notify_all` does. It is called after
the store the waiting threads wait for.

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

int main() {
    atomic<string> go;
    string none = go.load();
    atomic<int> started = 0;
    vector<thread> runners;
    for (int i : range(3)) {
        runners.emplace_back([&] {
            go.wait(none);
            ++started;
        });
    }
    go = string("go");
    go.notify_all();
    for (auto& r : runners) {
        r.join();
    }
    println("{} started on \"{}\"", started.load(), go.load());
}
```

Output:

```text
3 started on "go"
```

## See also

- [notify_one](notify_one.md): wakes one waiting thread
- [wait](wait.md): blocks while the handle holds the object given
- [sgcl::atomic\<H\>](README.md)
