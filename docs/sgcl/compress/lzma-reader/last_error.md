[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md) › [reader](../lzma-reader.md)

# sgcl::compress::lzma::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the error of the data, kept: the [compress::error](../error.md) whose code, offset and detail a read gave as an
`io::error` of the compress category. The read that reached it and every read after give it; a [reset](reset.md) clears
it. A failure of `in` itself is not kept here: the read returns `in`'s error as it came.

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
    auto packed = compress::lzma::compress("hello, hello, hello");
    packed.pop_back();  // cut short on the way

    compress::lzma::reader r{io::buffer(packed)};
    auto text = r.read_all_text();
    if (!text) {
        println("{}", text.error().message());
        println("{}", r.last_error()->message());
    }
}
```

Output:

```text
read lzma: unexpected end of data
offset 25: lzma: unexpected end of the compressed data
```

## See also

- [compress::error](../error.md)
- [sgcl::compress::lzma::reader](../lzma-reader.md)
