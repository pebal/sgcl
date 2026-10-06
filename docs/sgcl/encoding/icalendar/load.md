[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::load, async_load

```cpp
static expected<icalendar, error> load(const string& path);                         // (1)
static async::task<expected<icalendar, error>> async_load(string path) noexcept;    // (2)
```

The calendar of a file, read as [parse](parse.md) reads a stream with the default options:
`icalendar::load("team.ics")`.

1. Read now.
2. (1) for a task, the file read on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The calendar, or the [error](../error/README.md): [parse](parse.md)'s, with its line and column, for the text;
`io` without a place, `io_error()` saying why, for a file that does not open or read.

## Complexity

Linear in the length of the file.

## Exceptions

- (1) What a read of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("team.ics", "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nX-WR-CALNAME:Team\r\nEND:VCALENDAR\r\n");
    println(encoding::icalendar::load("team.ics")->text("X-WR-CALNAME", "?"));
    println(encoding::icalendar::load("missing.ics").error().message());
}
```

Output:

```text
Team
input/output error: open missing.ics: No such file or directory
```

## See also

- [save](save.md)
- [sgcl::encoding::icalendar](README.md)
