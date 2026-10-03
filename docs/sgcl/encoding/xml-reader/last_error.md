[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [reader](README.md)

# sgcl::encoding::xml::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

What stopped the reader, or `nullopt` while nothing has: a document that is not well formed, a limit of the
[options](../xml-options.md) passed, the stream failing (`errc::io`, the stream's error in `io_error()`), an
element [read](read.md) as a type it is not. Once it is set, every call to read gives nothing. The
[error](../error/README.md) has the byte of the input, the line, the column in characters and the path of the elements
open (`/feed/entry`); an error of a mapping has the path inside the element and the offset of its start.

## Parameters

None.

## Return value

The error, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<feed>\n  <entry>1</entry>\n  <entry>2</feed>");
    while (r.next()) {
    }
    if (auto& e = r.last_error()) {
        println("{}:{} {}", e->line(), e->column(), e->path());
        println(e->message());
    }
}
```

Output:

```text
3:11 /feed/entry
3:11 /feed/entry: </feed> closes <entry>
```

## See also

- [error](../error/README.md), [errc](../errc.md)
- [sgcl::encoding::xml::reader](README.md)
