[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](../sevenzip-writer.md)

# sgcl::compress::sevenzip::writer::add

```cpp
/*(1)*/ void add(const string& name, const slice<const byte>& data);
/*(2)*/ void add(const string& name, const slice<const byte>& data, const entry_info& info);
```

Writes a whole entry: a [create](create.md) of the name and its data. A text is its bytes, a `vector<byte>`
likewise. Nothing is returned: a failure is kept as the writer's first error, given by
[last_error](last_error.md) and by the [close](close.md).

1. A file, modified now, mode 0644.
2. With the times, the mode, the link and the attributes of `info`; with `symlink`, `data` is the link's target.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the entry's name, UTF-8, `/` between the parts |
| `data` | the entry's data |
| `info` | its times, mode, link, attributes ([entry_info](../sevenzip-entry_info.md)) |

## Return value

None.

## Complexity

Linear in the size of `data`.

## Exceptions

What the writes of the output throw; the errors are kept.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    w.add("notes.txt", "remember the milk\n");
    w.add("latest", "notes.txt", {.symlink = true});
    w.add("", "no name");
    println("{}", w.last_error()->message());
}
```

Output:

```text
7z: an entry name that is empty, has a NUL or is not UTF-8
```

## See also

- [create](create.md), [add_directory](add_directory.md)
- [add_file](add_file.md): a file as an entry
- [sgcl::compress::sevenzip::writer](../sevenzip-writer.md)
