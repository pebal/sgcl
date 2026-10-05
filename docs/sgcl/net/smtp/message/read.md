[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& buffer) const noexcept;       // (1)
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer)    // (2)
    const noexcept;
```

The next bytes of the message into `buffer`; 0 at its end. Nothing waits: the message is in memory.

1. On this thread or in a task.
2. The same as a task, for code written over `async_read`.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

How many bytes; 0 at the end.

## Complexity

Linear in the bytes given.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.handle([](net::smtp::message m) {
        vector<byte> head(8);
        size_t n = *m.read(head);
        println("{}", string(head.as_slice().first(n)));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    encoding::email sent("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");
    net::smtp::send(url, sent);
    srv.close();
    serving.wait();
}
```

Output:

```text
Date: Mo
```

## See also

- [bytes](bytes.md)
- [message](README.md)
