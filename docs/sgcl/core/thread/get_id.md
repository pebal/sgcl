[sgcl](../../README.md) › [core](../README.md) › [thread](README.md)

# sgcl::thread::get_id

```cpp
id get_id() const noexcept;
```

The id of the thread the object stands for: what `this_thread::get_id()` returns on that thread.

## Parameters

None.

## Return value

The thread's id, or a default-constructed `id` when the object stands for no thread.

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
    thread::id seen;
    thread worker([&seen] { seen = this_thread::get_id(); });
    thread::id id = worker.get_id();
    worker.join();
    println("{} {}", id == seen, id != this_thread::get_id());
    println("{}", worker.get_id() == thread::id());
}
```

Output:

```text
true true
true
```

## See also

- [native_handle](native_handle.md): the handle of the thread on the platform
- [sgcl::thread](README.md)
