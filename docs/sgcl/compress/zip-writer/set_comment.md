[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](README.md)

# sgcl::compress::zip::writer::set_comment

```cpp
expected<void, error> set_comment(const string& comment) noexcept;
```

Sets the archive's comment, which [close](close.md) writes into the end record; the archive's
[comment](../zip-archive/comment.md) reads it back. A comment past 65 535 bytes is `errc::invalid_argument`, kept as
the writer's first error, and so is a comment set after the close, which no end record would hold
(`"zip: set_comment after close"`).

## Parameters

| Parameter | Description |
|---|---|
| `comment` | the comment, at most 65 535 bytes |

## Return value

Nothing, or the [error](../error/README.md): a comment too long, a call after the close, or the error kept from before.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    println("{}", w.set_comment("nightly build").has_value());
    println("{}", w.set_comment(string("x").repeat(70000)).error().message());
}
```

Output:

```text
true
zip: comment longer than 65535 bytes
```

## See also

- [comment](../zip-archive/comment.md)
- [sgcl::compress::zip::writer](README.md)
