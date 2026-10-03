[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::compress_category

```cpp
#include "sgcl/compress/error.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    const std::error_category& compress_category() noexcept;
}
```

Returns the error category of the module's codes, named `"compress"`, whose messages are the codes' own words
(`"corrupt data"`, `"checksum mismatch"`, `"preset dictionary required"`). It is the category of the `error_code`
inside an `io::error` that a reader of the module gives when it is read as an [io stream](../io/README.md) and the
data fails, so a program that holds only the `io::error` tells a failure of the data from one of the stream.

## Parameters

None.

## Return value

The one category object of the module, the same on every call.

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
    auto packed = compress::gzip::compress("hello, hello, hello");
    packed[packed.size() - 8] ^= byte(1);

    compress::gzip::reader r{io::buffer(packed)};
    auto text = r.read_all_text();
    if (!text) {
        const io::error& e = text.error();
        println("{}", e.message());
        println("{}", &e.code().category() == &compress::compress_category());
    }
}
```

Output:

```text
read gzip: checksum mismatch
true
```

## See also

- [make_error_code](make_error_code.md): a code of the module as an `error_code`
- [errc](errc.md): the codes
- [sgcl::compress](README.md)
