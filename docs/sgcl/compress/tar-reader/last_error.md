[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [reader](../tar-reader.md)

# sgcl::compress::tar::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the whole of the last failure: its code, the byte of the archive where it was found, the entry it was in. A
read of the entry's data gives a failure as an `io::error` of the compress category; this keeps the
[compress::error](../error.md) behind it.

## Parameters

None.

## Return value

The last failure, or `nullopt` while there was none.

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
    compress::tar::writer w(archive);
    (void)w.write_header({.name = "big.bin", .size = 5000});
    (void)w.write(vector<byte>(5000));
    (void)w.close();

    auto cut = slice<const byte>(archive.data()).first(2048);  // cut short on the way
    compress::tar::reader r{io::buffer(cut)};
    (void)r.next();
    auto data = r.read_all();
    println("{}", data.error().message());
    println("{}", r.last_error()->message());
}
```

Output:

```text
read tar entry big.bin: unexpected end of data
offset 2048: tar: entry big.bin: unexpected end of data
```

## See also

- [compress::error](../error.md)
- [sgcl::compress::tar::reader](../tar-reader.md)
