[sgcl](../../README.md) › [slog](../README.md) › [logger](../logger.md)

# sgcl::slog::logger::flush

```cpp
void flush() const;
```

Writes now the batches of a buffered logger ([options](../options.md)`::buffered`): every worker's batch of its
output, each by one write. The collector's lines waiting for [collector_log](../collector_log.md) are logged first.
A logger that is not buffered has nothing waiting: its lines were written by their records.

Without a call, a batch is written when it is full, at a record of `warn` and up, when its worker goes to sleep and
at exit.

## Parameters

None.

## Return value

None. A write that fails is counted by [dropped](dropped.md).

## Complexity

Linear in the number of workers: each batch takes its lock and is written.

## Exceptions

What a writer of the program throws; a failed write throws nothing. The lines of a batch whose writer threw are lost,
and the batches stay usable.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::logger log(slog::options{.out = out, .buffered = true});
    log.info("one");
    log.info("two");
    println("before: {} bytes", out.size());
    log.flush();
    print(out.text());
}
```

Output:

```text
before: 0 bytes
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=one
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=two
```

## See also

- [dropped](dropped.md)
- [options](../options.md): `buffered`
- [sgcl::slog::logger](../logger.md)
