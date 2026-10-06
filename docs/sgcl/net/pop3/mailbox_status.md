[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md) › mailbox_status

# sgcl::net::pop3::mailbox_status

```cpp
#include "sgcl/net/pop3/types.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    struct mailbox_status {
        size_t messages = 0;
        uint64_t size = 0;
    };
}
```

`sgcl::net::pop3::mailbox_status` is what [status](client/status.md) gives, STAT's answer: the messages not marked deleted and their size. Compared member by member (`==`).

## Member objects

| Member | Description |
|---|---|
| `messages` | the messages not marked deleted |
| `size` | their size in bytes |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::pop3::mailbox_status st{2, 1024};
    println("{} {} {}", st.messages, st.size, st == net::pop3::mailbox_status{2, 1024});
}
```

Output:

```text
2 1024 true
```

## See also

- [client](client/README.md)
- [pop3](README.md)
