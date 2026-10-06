[sgcl](../README.md) › [net](README.md) › [connection](connection/README.md)

# sgcl::net::connection::received

```cpp
#include "sgcl/net/connection.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class connection {
    public:
        struct received {
            size_t size = 0;
            vector<io::file> files;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::connection::received` is what [receive_descriptors](connection/receive_descriptors.md) read: the
bytes, and the descriptors that came with them, each an [io::file](../io/file/README.md) the program owns.

## Member objects

| Field | Description |
|---|---|
| `size` | the bytes read into the buffer; 0 at the end of the stream |
| `files` | the descriptors that came with them, in their order, close-on-exec; none when the bytes came alone |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::unix_domain::listen("r.sock");
    net::connection a = net::unix_domain::connect("r.sock");
    net::connection b = l.accept();
    a.write("bytes alone");
    vector<byte> buffer(32);
    net::connection::received got = b.receive_descriptors(buffer).value();
    println("{} {}", got.size, got.files.size());
    l.close();
}
```

Output:

```text
11 0
```

## See also

- [receive_descriptors](connection/receive_descriptors.md)
- [sgcl::net::connection](connection/README.md)
