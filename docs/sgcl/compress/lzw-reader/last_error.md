[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw/README.md) › [reader](README.md)

# sgcl::compress::lzw::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the error of the data, kept: the [compress::error](../error/README.md) whose code, offset and detail a read gave as
an `io::error` of the compress category. The read that reached it and every read after give it. A
failure of `in` itself is not kept here: the read returns `in`'s error as it came.

## Parameters

None.

## Return value

The error of the data, or `nullopt` while there was none.

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
    auto packed = compress::lzw::compress("hello, hello, hello", compress::lzw::order::lsb, 8);
    packed.pop_back();  // cut short on the way

    compress::lzw::reader r{io::buffer(packed), compress::lzw::order::lsb, 8};
    auto text = r.read_all_text();
    if (!text) {
        println("{}", text.error().message());
        println("{}", r.last_error()->message());
    }
}
```

Output:

```text
read lzw: unexpected end of data
offset 16: lzw: unexpected end of the data (no end code)
```

## See also

- [compress::error](../error/README.md)
- [sgcl::compress::lzw::reader](README.md)
