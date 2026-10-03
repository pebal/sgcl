[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](../sevenzip-writer.md)

# sgcl::compress::sevenzip::writer::create

```cpp
/*(1)*/ io::writer create(const string& name) noexcept;
/*(2)*/ io::writer create(const string& name, const entry_info& info) noexcept;
```

Ends the entry before and starts a new one: gives an [io writer](../../io/writer.md) of its data, which goes into the
current folder through its coders as it is written (a new folder when the entry calls for another filter or the
folder is full). The next `create`, `add` or `add_directory` ends it; the entry writer's own `close()` does too, and
may be left out. A write to an entry that was ended is `io::errc::closed`, an error of that write alone, which the
writer does not keep. There is no error here: a name refused is
kept as the writer's first error, and the entry writer's writes give it. The entry writer's `async_write` encodes in
portions of 64 KB and lets the worker go between them.

1. A file, modified now, mode 0644.
2. With the times, the mode, the link and the attributes of `info`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the entry's name, UTF-8, `/` between the parts |
| `info` | its times, mode, link, attributes ([entry_info](../sevenzip-entry_info.md)) |

## Return value

The writer of the entry's data.

## Complexity

Constant, and the end of the entry before.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    io::writer log = w.create("app.log");
    for (string line : {"started\n", "stopped\n"}) {
        (void)log.write(line);
    }
    (void)w.create("other.log");  // ends app.log
    println("{}", log.write("too late\n").error().message());
    println("{}", w.close().has_value());
}
```

Output:

```text
write 7z: stream closed
true
```

## See also

- [add](add.md): a whole entry
- [sgcl::compress::sevenzip::writer](../sevenzip-writer.md)
