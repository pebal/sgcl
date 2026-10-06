[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::save, async_save

```cpp
expected<void, error> save(const string& path) const;                         // (1)
async::task<expected<void, error>> async_save(string path) const noexcept;    // (2)
```

The component's [to_string](to_string.md) into a file, made or written over.

1. Written now.
2. (1) for a task: the component copied into it, the file written on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or the [error](../error/README.md) `io`, without a place, for a file that does not open or write.

## Complexity

Linear in the size of the component.

## Exceptions

- (1) What [to_string](to_string.md) throws, and what a write of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::icalendar().save("empty.ics");
    print("{}", io::read_text("empty.ics").value_or(string("?")));
    println(encoding::icalendar().save("no/such/dir/a.ics").error().message());
}
```

Output:

```text
BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//sgcl//sgcl//EN
END:VCALENDAR
input/output error: open no/such/dir/a.ics: No such file or directory
```

## See also

- [load](load.md)
- [sgcl::encoding::icalendar](README.md)
