[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](README.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::notify_all

```cpp
void notify_all() noexcept;
```

Wakes every thread blocked in [wait](wait.md) on the word viewed, as `std::atomic_ref::notify_all` does. It is
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

struct Config {
    int version;
};

struct Shared {
    tracked_ptr<Config> config;
    atomic<int> started = 0;
};

int main() {
    tracked_ptr shared = make_tracked<Shared>();
    vector<thread> readers;
    for (int i : range(3)) {
        readers.emplace_back([shared] {
            atomic_ref(shared->config).wait(nullptr);
            shared->started += atomic_ref(shared->config).load()->version;
        });
    }
    atomic_ref config(shared->config);
    config = make_tracked<Config>(1);
    config.notify_all();
    for (auto& r : readers) {
        r.join();
    }
    println("{} readers started", shared->started.load());
}
```

Output:

```text
3 readers started
```

## See also

- [notify_one](notify_one.md): wakes one waiting thread
- [wait](wait.md): blocks while the pointer is the one given
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](README.md)
