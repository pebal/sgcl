[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::handle, async_handle

```cpp
optional<string> handle(const string& text) const;                         // (1)
async::task<optional<string>> async_handle(string text) const noexcept;    // (2)
```

A message's text — a request, a notification, a batch — answered by the table, as JSON-RPC 2.0 answers it: the
response's text, none when there is nothing to answer (notifications only), a parse error or an invalid request for
a message that is not one. For a transport of the program's own. When every method a message calls answers at once,
`handle` does it on the calling thread, with no task.

`handle` waits on the calling thread; a task awaits `async_handle`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the message |


## Return value

The response's text, or none.

## Complexity

Linear in the message.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    println("{}", m.handle(R"([{"jsonrpc":"2.0","method":"add","params":[1,1],"id":1},{"jsonrpc":"2.0","method":"nope","id":2}])").value());
    println("{}", m.handle("{not json").value());
}
```

Output:

```text
[{"jsonrpc":"2.0","id":1,"result":2},{"jsonrpc":"2.0","id":2,"error":{"code":-32601,"message":"Method not found"}}]
{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}
```

## See also

- [async_serve](async_serve.md)
- [methods](README.md)
