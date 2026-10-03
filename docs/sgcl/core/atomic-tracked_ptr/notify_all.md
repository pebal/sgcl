[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](README.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::notify_all

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

struct Config {
    int version;
};

int main() {
    atomic<tracked_ptr<Config>> config;
    atomic<int> started = 0;
    vector<thread> readers;
    for (int i : range(3)) {
        readers.emplace_back([&] {
            config.wait(nullptr);
            started += config.load()->version;
        });
    }
    config = make_tracked<Config>(1);
    config.notify_all();
    for (auto& r : readers) {
        r.join();
    }
    println("{} readers started", started.load());
}
```

Output:

```text
3 readers started
```

## See also

- [notify_one](notify_one.md): wakes one waiting thread
- [wait](wait.md): blocks while the pointer is the one given
- [sgcl::atomic\<tracked_ptr\<T\>\>](README.md)
