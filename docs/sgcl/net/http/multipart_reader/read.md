[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [multipart_reader](README.md)

# sgcl::net::http::multipart_reader::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept;    // (2)
```

Reads the current part's content into `buffer`: at most `buffer.size()` bytes, and at most the 32 KB of the window,
at least one while there are any; 0 at the content's end (the delimiter is next's to read), before the first
[next](next.md) and after the last part. Go's `Part.Read`. The bytes of a delimiter cut at the window's end are held
back until the next bytes tell whether it is one.

1. Reads the body on the calling thread; over a request's body, for a thread of the program, never a handler.
2. Returns a task that does the same: a handler writes `co_await parts.async_read(b)`, or `co_await
   parts.async_read_all()`, or `co_await io::async_copy(file, parts)`.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read; or the [io::error](../../../io/error/README.md) that ends the reader:
`io::errc::unexpected_eof` for a body that ends before its close delimiter, or the stream's own error.

## Complexity

Linear in the bytes read and in the bytes searched for the delimiter.

## Exceptions

- (1) `std::system_error` when the stream is a request's body and the wait starts the scheduler and a worker's thread
  cannot be started.
- (2) None.

## Example

A file's part copied to a file, 32 KB at a time:

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string body = "--b\r\nContent-Disposition: form-data; name=\"f\"; filename=\"big.bin\"\r\n\r\n" +
                  string(1000000, 'z') + "\r\n--b--\r\n";
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), "b");
    net::http::multipart_reader::part p = parts.next().value().value();
    io::file out = io::create(p.filename).value();
    println("{}", io::copy(out, parts).value());
    out.close();

    vector<byte> room(10);
    println("{}", parts.read(room).value());
}
```

Output:

```text
1000000
0
```

## See also

- [next, async_next](next.md): the next part
- [io::copy](../../../io/copy.md): a stream to a writer
- [multipart_reader](README.md)
