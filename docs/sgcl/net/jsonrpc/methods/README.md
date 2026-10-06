[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md)

# sgcl::net::jsonrpc::methods

```cpp
#include "sgcl/net/jsonrpc/methods.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    class methods;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::methods` is the table of methods a [peer](../peer/README.md) or an HTTP route serves, by name:
handlers that answer at once, handlers that wait (tasks, cancellable), and notifications. It answers the
specification's whole of a message — a request, a notification, a batch, the errors of a message that is not one.

## Rules

- A handle of one word: copies share the table, which may change while it serves.
- A handler that answers at once runs where the message is read (on a peer's reading task, in an HTTP handler); one
  added with [add_task](add_task.md) runs as a task of its own, so that a slow method holds nothing else up.
- A method called as a notification runs and its result is not sent (§4.1); a notification no method takes is
  passed over.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](methods.md) | an empty table, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another table |
| [add](add.md) | a method that answers at once |
| [add_task](add_task.md) | a method that waits |
| [add_notification](add_notification.md) | a notification |
| [remove](remove.md) | a method taken out |
| [handle, async_handle](handle.md) | a message's text answered |
| [async_serve](async_serve.md) | the methods as an HTTP handler |
| [operator bool](operator_bool.md) | whether the handle holds a table |
| [operator==](operator_cmp.md) | whether two handles are the same table |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"

using namespace sgcl;

int main() {
    net::jsonrpc::methods m;
    m.add("add", [](const encoding::json& p) -> expected<encoding::json, net::jsonrpc::error> {
        return encoding::json(p[0].as_int(0) + p[1].as_int(0));
    });
    println("{}", m.handle(R"({"jsonrpc":"2.0","method":"add","params":[2,3],"id":1})").value());
}
```

Output:

```text
{"jsonrpc":"2.0","id":1,"result":5}
```

## See also

- [peer](../peer/README.md)
- [error](../error.md)
