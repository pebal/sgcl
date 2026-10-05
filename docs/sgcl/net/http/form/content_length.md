[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::content_length

```cpp
expected<uint64_t, io::error> content_length() const noexcept;
```

The body's length in bytes: the heads of the parts, the fields' texts, the files' sizes taken now, the delimiters.
What a client sends as `Content-Length`, taken again when the send begins.

## Parameters

None.

## Return value

The length; or the [io::error](../../../io/error/README.md) of a file that cannot be read: `ENOENT` for one that is
not there, `EISDIR` for a directory.

## Complexity

Linear in the number of parts, a `stat` of each file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::write_file("ten.bin", "0123456789");
    net::http::form f{net::http::form::file("f", "ten.bin")};
    uint64_t before = f.content_length().value();
    io::write_file("ten.bin", "01234567890123456789");
    println("{}", f.content_length().value() - before);

    net::http::form missing{net::http::form::file("f", "nowhere.bin")};
    println("{}", missing.content_length().error().code() == std::errc::no_such_file_or_directory);
}
```

Output:

```text
10
true
```

## See also

- [reader](reader.md): the body itself
- [form](README.md)
