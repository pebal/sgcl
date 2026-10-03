[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::shared_memory

```cpp
#include "sgcl/io/shared_memory.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class shared_memory final;
}
```

`io::shared_memory` is a region of memory shared between processes under a name: one process makes it with
[create](create.md), others map it with [open](open.md), and what one writes into
[data](data.md) the others read there, with no copy and no call between them (`shm_open` and `mmap`
on POSIX, `CreateFileMappingW` and `MapViewOfFile` on Windows). [remove](remove.md) takes the name
away. Neither Go's standard library nor `std` has such a region.

A `shared_memory` is a handle of one word, a `tracked_ptr` to the region inside, the same kind of region an
[io::mapping](../mapping/README.md) holds: a copy is the same region. `data()` is the region's bytes as a `slice<byte>` whose
owner is the region, so a slice kept after the last handle is gone still reads it: the region is unmapped when
nothing holds it any more, by the destructor on the collector's thread, or given back at once by
[close](close.md). The region is always read and written, so there is only `data()`, a `slice<byte>`
(a `mapping` has `data()` as a `slice<const byte>` and `writable_data()` for a writable one).

The name is the program's own word: not empty, and without `/` or `\`. On POSIX it becomes `/name` for `shm_open`
(macOS takes at most 30 characters, a longer name answers `ENAMETOOLONG`); on Windows `Local\name`, the namespace
of the session, since `Global\` needs a privilege an ordinary program does not have.

## Rules

- The region is outside the managed heap: put only trivial data in it — never a tracked_ptr or a library handle.
- Lifetime and cleanup. POSIX: the object and its name live until [remove](remove.md), past the end
  of every process that used it, until a reboot; a program that creates one removes it when done, and one that may
  find a leftover from a crash removes the name before [create](create.md). A process that mapped the
  object keeps it after `remove`; a `create` of the name then makes a new one. Windows: the object lives until the
  last handle to it, a mapped view included, is closed; `remove(name)` has nothing to do there and succeeds.
- The object is made readable and writable by the user's own processes alone (`0600`), and its bytes are zeros.
- The processes see each other's writes at once, with no ordering beyond what they make themselves: a flag written
  after the data is an `std::atomic` in the region (lock-free, so trivial), stored with release and loaded with
  acquire. Inter-process mutexes and semaphores are not part of io.
- A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted\<io::shared_memory\>](../../core/rooted/README.md), as a slice of the region is a `rooted<slice<byte>>` there. It
  is a [req::handle](../../core/req/handle.md).
- A `shared_memory` made by its default constructor holds no region (`!s`); an operation on it is a contract
  violation, asserted in a debug build.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](shared_memory.md) | an empty handle |
| [create](create.md) | makes a new object under a name and maps it |
| [open](open.md) | maps the object of a name |
| [remove](remove.md) | takes the name away |

#### Element access

| Function | Description |
|---|---|
| [data](data.md) | the region's bytes |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the length of the region |

#### File operations

| Function | Description |
|---|---|
| [close](close.md) | gives the region back now |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | checks whether the region was closed |
| [operator bool](operator_bool.md) | checks whether the handle holds a region |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same region |

## Example

Two views of one named object: what one writes, the other reads.

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example");  // a leftover of an earlier run, if any
    io::shared_memory made = io::shared_memory::create("sgcl-example", 64);
    io::shared_memory opened = io::shared_memory::open("sgcl-example");
    const char text[] = "hello";
    for (size_t i : range(5)) {
        made.data()[i] = byte(text[i]);
    }
    println("{}", string(opened.data().first(5)));
    (void)io::shared_memory::remove("sgcl-example");
}
```

Output:

```text
hello
```

## See also

- [mapping](../mapping/README.md): a file mapped, the same region under another handle
- [command](../command/README.md): starting the other process
- `tests/io/mapping.cpp`: create, open and remove; the errors (a taken name, a missing one, bad names, a size of
  0); two processes through `io::command`, the child opening the name and writing; `rooted<io::shared_memory>` in a
  std container and a `rooted<slice<byte>>` holding the region alone through collections
