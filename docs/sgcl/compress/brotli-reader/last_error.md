[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md) › [reader](README.md)

# sgcl::compress::brotli::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the error of the data, kept: the [compress::error](../error/README.md) whose code, offset and detail a read gave as an
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
    auto packed = compress::brotli::compress("hello, hello, hello");
    packed.pop_back();  // cut short on the way

    compress::brotli::reader r{io::buffer(packed)};
    auto text = r.read_all_text();
    if (!text) {
        println("{}", text.error().message());
        println("{}", r.last_error()->message());
    }
}
```

Output:

```text
read brotli: unexpected end of data
offset 17: brotli: unexpected end of data
```

## See also

- [compress::error](../error/README.md)
- [sgcl::compress::brotli::reader](README.md)
