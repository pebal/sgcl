[sgcl](../../README.md) › [slog](../README.md) › [syslog](README.md)

# sgcl::slog::syslog::dropped

```cpp
uint64_t dropped() const noexcept;
```

Returns the messages whose write failed: a full local queue, a connection broken. A handler returns nothing to the
logger, so this is where a lost message shows.

## Parameters

None.

## Return value

The count.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct Broken {
    expected<size_t, io::error> write(const slice<const byte>&) {
        return unexpected(io::error(io::errc::closed, "write"));
    }
};

int main() {
    Broken b;
    slog::syslog h(io::writer(b), {.app_name = "a", .hostname = "h"});
    slog::logger log(h);
    log.info("lost");
    println("{}", h.dropped());
}
```

Output:

```text
1
```

## See also

- [handle](handle.md)
- [sgcl::slog::syslog](README.md)
