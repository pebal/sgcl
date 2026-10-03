[sgcl](../../README.md) › [io](../README.md) › [shared_memory](../shared_memory.md)

# sgcl::io::shared_memory::open

```cpp
static expected<shared_memory, error> open(const string& name) noexcept;
```

Maps the object of the name for reading and writing: one a [create](create.md) made, in this process or another.
Every open is a region of its own over the same object, so two opens of one name are two handles that differ, and
what is written through one is read through the other.

Its [size](size.md) is the object's as the system keeps it: the size given to `create` on Linux, rounded up to a
page on macOS (16 KB on Apple silicon) and on Windows. A protocol between the processes that needs the exact length
keeps it in the region.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name the object was created under |

## Return value

The handle of the mapped region, or an [error](../error.md) with the operation `open` and the name as its path:

- `is_not_found()` when there is no object of the name;
- `errc::invalid_path` for a bad name (empty, or with `/` or `\`);
- the error of `shm_open` or `mmap` otherwise (the operation `mmap` for the last).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-open");
    io::shared_memory made = io::shared_memory::create("sgcl-example-open", 100);
    io::shared_memory opened = io::shared_memory::open("sgcl-example-open");
    made.data()[5] = byte(42);
    println("{} {} {}", int(opened.data()[5]), opened.size() >= 100, opened == made);
    (void)io::shared_memory::remove("sgcl-example-open");
    println("{}", io::shared_memory::open("sgcl-example-open").error().message());
}
```

Output:

```text
42 true false
open sgcl-example-open: No such file or directory
```

## See also

- [create](create.md): the object made
- [remove](remove.md): the name taken away
- [sgcl::io::shared_memory](../shared_memory.md)
