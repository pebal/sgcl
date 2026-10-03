[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md) › [dumper](README.md)

# sgcl::encoding::operator== (sgcl::encoding::hex::dumper)

```cpp
friend bool operator==(const dumper& a, const dumper& b) noexcept;
```

Whether two handles share one stream: a copy and the dumper it was copied from are equal, two dumpers made by
two calls of [dumper_to](../hex/dumper_to.md) are not, even over one writer. Two that hold none are equal.
`!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the dumpers compared |

## Return value

`true` when the two hold the same stream, or both none.

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
    io::buffer out;
    encoding::hex::dumper a = encoding::hex::dumper_to(out);
    encoding::hex::dumper b = a;
    encoding::hex::dumper c = encoding::hex::dumper_to(out);
    println("{} {} {}", a == b, a == c, encoding::hex::dumper() == encoding::hex::dumper());
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](hex-dumper.md): a copy that shares the stream
- [sgcl::encoding::hex::dumper](README.md)
