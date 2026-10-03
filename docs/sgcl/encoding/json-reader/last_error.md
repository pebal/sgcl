[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [reader](README.md)

# sgcl::encoding::json::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

The error the reader stopped at: what tells an error from the end of the input, when a method returned `nullopt`
or `false`. The [error](../error/README.md) has the [code](../errc.md), the offset of the byte in the whole input, the
line and the column (in code points), counted across the blocks the reader let go, the path inside the value for
a [typed read](read.md), and the error of the stream when the stream failed (`errc::io`). The reader keeps the
first error: every call after it returns `nullopt` or `false`, and the error stays as it was.

## Parameters

None.

## Return value

The first error, or `nullopt` when there was none.

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
    encoding::json::reader r(string("{\"a\": [1, 2],\n \"b\": tru}"));
    while (r.next()) {
    }
    const auto& e = r.last_error();
    println("{} {} {}:{}", e->code() == encoding::errc::syntax, e->offset(), e->line(), e->column());
    println(e->message());

    encoding::json::reader done(string("[1]"));
    while (done.next()) {
    }
    println("{}", done.last_error().has_value());

    encoding::json::reader broken(io::reader([](slice<byte> b) -> expected<size_t, io::error> {
        return unexpected(io::error(std::make_error_code(std::errc::connection_reset), "read"));
    }));
    println("{}", broken.next().has_value());
    println(broken.last_error()->message());
}
```

Output:

```text
true 23 2:10
2:10: invalid character '}' in the literal true
false
false
1:1: input/output error: read: Connection reset by peer
```

## See also

- [error](../error/README.md), [errc](../errc.md): the codes and the place
- [offset](offset.md): where the reader is
- [sgcl::encoding::json::reader](README.md)
