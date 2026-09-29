# sgcl::io::shared_memory

```cpp
#include "sgcl/io/shared_memory.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    class shared_memory final {   // a handle of one word
    public:
        static expected<shared_memory, error> create(const string& name, size_t size);
        static expected<shared_memory, error> open(const string& name);
        static expected<void, error> remove(const string& name);
        slice<byte> data() const noexcept;
        size_t size() const noexcept;
        expected<void, error> close() const;
    };
}
```

A region of memory shared between processes under a name: one process makes it with `create(name, size)`, others map it with `open(name)`, and what one writes into `data()` the others read there, with no copy and no call between them (`shm_open` and `mmap` on POSIX, `CreateFileMappingW` and `MapViewOfFile` on Windows).

A `shared_memory` is a handle of one word, a `tracked_ptr` to the region inside, the same kind of region an [`io::mapping`](mapping.md) holds: a copy is the same region. `data()` is the region's bytes as a `slice<byte>` whose owner is the region, so a slice kept after the last handle is gone still reads it: the region is unmapped when nothing holds it any more, by the destructor on the collector's thread. The region is always read and written, so there is only `data()`, a `slice<byte>` (a `mapping` has `data()` as a `slice<const byte>` and `writable_data()` for a writable one).

The name is the program's own word: not empty, and without `/` or `\`. On POSIX it becomes `/name` for `shm_open` (macOS takes at most 30 characters, a longer name answers `ENAMETOOLONG`); on Windows `Local\name`, the namespace of the session, since `Global\` needs a privilege an ordinary program does not have.

## Rules

- The region is outside the managed heap: put only trivial data in it — never a tracked_ptr or a library handle.
- Lifetime and cleanup. POSIX: the object and its name live until `remove(name)`, past the end of every process that used it, until a reboot; a program that creates one removes it when done, and one that may find a leftover from a crash removes the name before `create`. A process that mapped the object keeps it after `remove`; a `create` of the name then makes a new one. Windows: the object lives until the last handle to it, a mapped view included, is closed; `remove(name)` has nothing to do there and succeeds.
- `create` fails with `is_exists()` when the name is taken, and with `std::errc::invalid_argument` for a size of 0; the new region is zeros. It is made readable and writable by the user's own processes alone (`0600`).
- `open` fails with `is_not_found()` when there is no object of the name. Its `size()` is the object's as the system keeps it: the size given to `create` on Linux, rounded up to a page on macOS (16 KB on Apple silicon) and on Windows. A protocol between the processes that needs the exact length keeps it in the region.
- A bad name answers `errc::invalid_path` from all three.
- The processes see each other's writes at once, with no ordering beyond what they make themselves: a flag written after the data is an `std::atomic` in the region (lock-free, so trivial), stored with release and loaded with acquire. Inter-process mutexes and semaphores are not part of io.

## Members

```cpp
shared_memory() noexcept;                                     // no region: !s
static expected<shared_memory, error> create(const string& name, size_t size);   // a new object, mapped; zeros
static expected<shared_memory, error> open(const string& name);                  // an existing object, mapped
static expected<void, error> remove(const string& name);                         // the name taken away (shm_unlink); nothing on Windows
slice<byte> data() const noexcept;                            // the region's bytes; empty once closed
size_t size() const noexcept;                                 // 0 once closed
expected<void, error> close() const;  bool is_closed() const noexcept;   // as mapping::close(): the region given back, an old slice reads zeros; the name kept until remove
explicit operator bool() const noexcept;                      // whether the handle holds a region
friend bool operator==(const shared_memory& a, const shared_memory& b) noexcept;   // the same region (two opens of one name are two)
```

## Example

```cpp
#include "sgcl/core/core.h"
#include "sgcl/io/io.h"

using namespace sgcl;

// Two views of one named object: what one writes, the other reads
int main() {
    io::shared_memory::remove("sgcl-example");                // a leftover of an earlier run, if any
    io::shared_memory made = io::shared_memory::create("sgcl-example", 64);
    io::shared_memory opened = io::shared_memory::open("sgcl-example");
    const char text[] = "hello";
    for (size_t i : range(5)) {
        made.data()[i] = byte(text[i]);
    }
    println("{}", string(opened.data().first(5)));
    io::shared_memory::remove("sgcl-example");
}
```

Output:

```text
hello
```

## See also

- [mapping](mapping.md): a file mapped, the same region under another handle
- [exec](exec.md): starting the other process
- `tests/io/mapping.cpp`: create, open and remove; the errors (a taken name, a missing one, bad names, a size of 0); two processes through `io::command`, the child opening the name and writing; `rooted<io::shared_memory>` in a std container and a `rooted<slice<byte>>` holding the region alone through collections.
