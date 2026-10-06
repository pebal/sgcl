[sgcl](../../README.md) › [net](../README.md) › jsonrpc

# sgcl::net::jsonrpc

```cpp
#include "sgcl/net/jsonrpc.h"   // namespace sgcl::net::jsonrpc
```

JSON-RPC 2.0 both ways, on [encoding::json](../../encoding/json/README.md). A table of [methods](methods/README.md)
serves calls; a [peer](peer/README.md) speaks over a byte stream — LSP's `Content-Length` framing, or one JSON a
line — or a WebSocket, calling the other side and serving it at once; a [client](client/README.md) calls a service
over HTTP, and the methods are an HTTP handler of a route. Batches, notifications, cancellation. Go has JSON-RPC 1.0
in `net/rpc/jsonrpc`; LSP's libraries carry 2.0 with its framing.

## The rules

1. A handler takes the call's params (an array, an object, or null when there are none) and gives a result or an
   [error](error.md) object; one added with [add](methods/add.md) answers at once, one added with
   [add_task](methods/add_task.md) is a task that may wait, given the call's stop token.
2. A call that fails on the other side is an error of the category `"jsonrpc"` whose value is the error object's
   code ([errc](errc.md) names the specification's); [error_of](error_of.md) gives the object back with its data.
3. A call whose stop token is stopped fails at once with `ECANCELED`, and the other side is sent
   `$/cancelRequest` (LSP's), which stops the handler's own token; a call past its timeout fails with `ETIMEDOUT`.
4. Requests that come in to a peer are served as they come, their responses written as they are ready (the
   specification does not order them); a batch's answers go back together in one array.
5. Every call that waits has two forms, `call()` on a thread and `co_await async_call()` in a task.
6. [methods](methods/README.md), [peer](peer/README.md) and [client](client/README.md) are handles of one word.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"jsonrpc"` |
| [error_of](error_of.md) | `error.h` | the error object a call's error carries |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |

## Classes

| Class | Header | Description |
|---|---|---|
| [batch_entry](batch_entry.md) | `peer.h` | one call or notification of a batch |
| [call_options](call_options.md) | `peer.h` | a call's timeout and stop token |
| [client](client/README.md) | `client.h` | JSON-RPC over HTTP POST |
| [error](error.md) | `error.h` | an error object: code, message, data |
| [methods](methods/README.md) | `methods.h` | the methods a peer or an HTTP route serves |
| [peer](peer/README.md) | `peer.h` | JSON-RPC over a stream or a WebSocket, both ways |
| [peer::options](peer-options.md) | `peer.h` | the framing, the methods served, the cancel method, the size limit |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the error codes of JSON-RPC 2.0 |
| [framing](framing.md) | `peer.h` | how messages are cut out of a stream |

## See also

- [encoding::json](../../encoding/json/README.md)
- [http](../http/README.md)
- [Benchmarks](../benchmarks.md)
- JSON-RPC 2.0 (jsonrpc.org/specification), LSP's base protocol; `tests/net/jsonrpc/` (a Python peer of LSP's framing)
