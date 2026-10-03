[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [dumper](../hex-dumper.md)

# sgcl::encoding::hex::dumper::dumper

```cpp
/*(1)*/ dumper() noexcept;
/*(2)*/ dumper(const dumper& other) noexcept;
```

1. A dumper that holds no stream: `!d` is `true`, and any other operation on it is a contract violation,
   checked by `assert`. A dumper with a stream is made by [hex::dumper_to](../hex/dumper_to.md).
2. A handle of the stream `other` holds: the two are one dumper, and what one writes the other's `close()`
   ends.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the dumper whose stream is shared |

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
    encoding::hex::dumper none;
    println("{}", !none);

    io::buffer out;
    encoding::hex::dumper wire = encoding::hex::dumper_to(out);
    encoding::hex::dumper copy = wire;
    wire.write("h");
    copy.write("i");
    copy.close();
    print(out.text());
    println("{}", wire.is_closed());
}
```

Output:

```text
true
00000000  68 69                                             |hi|
true
```

## See also

- [dumper_to](../hex/dumper_to.md): a dumper with a stream
- [operator==](operator_cmp.md): whether two handles share one stream
- [sgcl::encoding::hex::dumper](../hex-dumper.md)
