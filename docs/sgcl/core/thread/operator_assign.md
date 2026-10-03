[sgcl](../../README.md) › [core](../README.md) › [thread](../thread.md)

# sgcl::thread::operator=

```cpp
/*(1)*/ thread& operator=(thread&& o) noexcept;
/*(2)*/ thread& operator=(const thread&) = delete;
```

1. Takes the thread of `o` over; `o` stands for no thread after. When this object still stands for a joinable
   thread, the program ends with `std::terminate`, as with `std::thread`: a thread is joined or detached before its
   object is given another.
2. A thread object is not copyable.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the thread object taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    atomic<int> runs = 0;
    thread current;
    for (int i : range(3)) {
        if (current.joinable()) {
            current.join();  // before the object is given the next thread
        }
        current = thread([&runs] { ++runs; });
    }
    current.join();
    println("{}", runs.load());
}
```

Output:

```text
3
```

## See also

- [(constructor)](thread.md): constructs a thread object
- [joinable](joinable.md): checks whether the object stands for a thread
- [sgcl::thread](../thread.md)
