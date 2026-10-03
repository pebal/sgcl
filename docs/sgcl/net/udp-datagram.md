[sgcl](../README.md) › [net](README.md) › [udp](udp/README.md) › datagram

# sgcl::net::udp::datagram

```cpp
#include "sgcl/net/socket.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct udp {
        struct datagram {
            size_t size = 0;
            endpoint from;
            bool truncated = false;
        };
    };
}
```

`sgcl::net::udp::datagram` is what [receive_from](udp-socket/receive_from.md) says of a datagram it received: how
many bytes the buffer got, who sent it, and whether it was longer than the buffer and cut. The bytes themselves are
in the buffer given to the receive. Go's `ReadFrom` returns the size and the sender as `(n, addr, err)`, and a
datagram cut only through the flags of `ReadMsgUDP`; here it is a field. A plain value, trivially copyable.

## Member objects

| Member | Description |
|---|---|
| `size` | the bytes of the datagram in the buffer, at most its size; `0` by default |
| `from` | the sender, an [endpoint](endpoint/README.md); an IPv4 peer of a socket of both families is reported as IPv4 |
| `truncated` | the datagram was longer than the buffer, and the rest of it is lost (`MSG_TRUNC`); `false` by default |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket server = net::udp::bind(":0");  // both families
    net::udp::socket client = net::udp::bind("127.0.0.1:0");
    net::endpoint to(net::ip_address::loopback_v4(), server.local_endpoint().port());
    client.send_to("twelve bytes", to);
    client.send_to("short", to);

    vector<byte> room(8);
    for (int i : range(2)) {
        net::udp::datagram d = server.receive_from(room).value();
        println("{} {} {}", d.size, d.truncated, d.from.address());
    }
}
```

Output:

```text
8 true 127.0.0.1
5 false 127.0.0.1
```

## See also

- [udp::socket](udp-socket/README.md): what receives it
- [receive_from, async_receive_from](udp-socket/receive_from.md): what gives it
