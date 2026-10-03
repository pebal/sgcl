[sgcl](../../README.md) › [slog](../README.md) › [logger](../logger.md)

# sgcl::slog::logger::dropped

```cpp
uint64_t dropped() const noexcept;
```

Returns the number of records whose write failed. A write that fails is not returned to the caller of a verb, as
`print`'s is not: it is counted here, and the first failure of an output is said in one line on `io::stderr`,
`sgcl::slog: a write failed: ` and the error's message. A batch of a buffered logger that fails counts every record
in it, and so does one whose writer of the program throws where no caller takes the exception: written by a worker
on its way to sleep or at exit. The count belongs to the output, so the copies and the children of a logger share it.

## Parameters

None.

## Return value

The records lost since the output was made.

## Complexity

Constant: one relaxed atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::file read_only = io::open("/dev/null").value();
    slog::logger log(read_only);
    log.info("first");
    log.with("k", 1).info("second");
    println("{} records lost", log.dropped());
}
```

Output:

```text
sgcl::slog: a write failed: write /dev/null: Bad file descriptor
2 records lost
```

## See also

- [flush](flush.md)
- [sgcl::slog::logger](../logger.md)
