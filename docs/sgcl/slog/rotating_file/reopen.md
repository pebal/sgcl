[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::reopen

```cpp
expected<void, io::error> reopen() const;
```

Opens the path again and writes there from now: for a tool that moved the file away (logrotate without
`copytruncate`, then a signal). The writes until then went on into the moved file, through its descriptor. What
`reopen_on_sighup` calls at the signal.

## Parameters

None.

## Return value

Nothing, or the `io::error` of the open; `errc::closed` after [close](close.md).

## Complexity

An open and a stat.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::rotating_file out = slog::rotating_file::open("app.log");
    out.write("one\n");
    io::rename("app.log", "app.log.1");  // what logrotate does
    out.reopen();
    out.write("two\n");
    println("{} {}", io::read_text("app.log.1")->trim(), io::read_text("app.log")->trim());
}
```

Output:

```text
one two
```

## See also

- [rotation](../rotation.md): reopen_on_sighup
- [sgcl::slog::rotating_file](README.md)
