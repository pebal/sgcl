[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::errc

```cpp
#include "sgcl/net/amqp/error.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    enum class errc {
        content_too_large = 311,
        no_route = 312,
        no_consumers = 313,
        connection_forced = 320,
        invalid_path = 402,
        access_refused = 403,
        not_found = 404,
        resource_locked = 405,
        precondition_failed = 406,
        frame_error = 501,
        syntax_error = 502,
        command_invalid = 503,
        channel_error = 504,
        unexpected_frame = 505,
        resource_error = 506,
        not_allowed = 530,
        not_implemented = 540,
        internal_error = 541,
        malformed = 1000,
        nacked
    };
}
```

The reply codes of AMQP 0-9-1 (§1.9) by their values, and the client's own failures past them, in the category
`"amqp"` ([category](category.md)); an error's path is the broker's text ("NOT_FOUND - no queue 'x' in vhost '/'").

| Value | Description |
|---|---|
| `content_too_large` | 311, "content too large": a message the broker will not take |
| `no_route` | 312, "no route": a mandatory publication no queue takes (a [returned](returned.md)'s code) |
| `no_consumers` | 313, "no consumers": an immediate publication no consumer takes |
| `connection_forced` | 320, "connection forced": the broker closed the connection (shut down, an operator) |
| `invalid_path` | 402, "invalid path": a vhost that is not there |
| `access_refused` | 403, "access refused": credentials or a permission refused |
| `not_found` | 404, "not found": an exchange or a queue that is not there |
| `resource_locked` | 405, "resource locked": another connection's exclusive queue |
| `precondition_failed` | 406, "precondition failed": a declaration unlike the one there is, an unknown delivery tag |
| `frame_error` | 501, "frame error": a frame the peer could not read |
| `syntax_error` | 502, "syntax error": a frame's values out of range |
| `command_invalid` | 503, "command invalid": a method out of place |
| `channel_error` | 504, "channel error": a channel used wrongly, every channel in use |
| `unexpected_frame` | 505, "unexpected frame": a frame out of place |
| `resource_error` | 506, "resource error": the broker out of a resource |
| `not_allowed` | 530, "not allowed": an operation the broker does not allow |
| `not_implemented` | 540, "not implemented": a method or a version the broker does not have |
| `internal_error` | 541, "internal error": the broker failed |
| `malformed` | 1000, "malformed AMQP frame": a frame of the broker's that breaks the specification, which ends the connection |
| `nacked` | 1001, "publication nacked": a publication the broker refused under publisher confirms |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    auto r = ch.declare_queue("nowhere", {.passive = true});
    println("{}", r.error().code() == net::amqp::errc::not_found);
    println("{}", r.error().path());
}
```

Output:

```text
true
NOT_FOUND - no queue 'nowhere' in vhost '/'
```

## See also

- [category](category.md)
