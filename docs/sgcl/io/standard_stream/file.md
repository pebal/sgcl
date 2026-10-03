[sgcl](../../README.md) › [io](../README.md) › [standard_stream](../standard_stream.md)

# sgcl::io::standard_stream::file

```cpp
io::file file() const noexcept;
```

Returns the [file](../file.md) over the stream's descriptor, made on the first use of the stream: for what takes a
file rather than a stream, a child's standard stream shared with the program's (`cmd.out = io::stdout.file()`), a
[stat](../file/stat.md) of the descriptor. The file is the stream's own and lives as long as the process; the
handle returned is a copy of it.

## Parameters

None.

## Return value

The file over the descriptor.

## Complexity

Constant.

## Exceptions

None.

## Notes

The file is never closed by the stream. A [close](../file/close.md) through the handle closes the descriptor of the
process itself: what was the standard output writes nowhere from then on.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file out = io::stdout.file();
    println("{}", out.fd());
    (void)out.write("written through the file\n");
}
```

Output:

```text
1
written through the file
```

## See also

- [command](../command.md): `in`, `out`, `err`, where a file is inherited as a descriptor
- [fd](fd.md): the descriptor
- [sgcl::io::standard_stream](../standard_stream.md)
