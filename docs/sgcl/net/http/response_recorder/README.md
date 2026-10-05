[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::response_recorder

```cpp
#include "sgcl/net/http/test.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class response_recorder;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::http::response_recorder` runs a handler with no network, Go's `httptest.ResponseRecorder`: its
[writer](writer.md) is a [response_writer](../response_writer/README.md) whose flushes and end send nothing and keep
everything, handed to a handler with a [test_request](../test_request.md); what the handler wrote is read back — the
[status](status.md), the [fields](headers.md), the [body](body.md), the [trailers](trailers.md), the
[informational](informational.md) responses and the count of [flushes](flushes.md). [serve](serve.md) runs a request
through the routes of a [server](../server/README.md), as the server routes it, without a socket.

Against Go, the recorder is not the writer itself (a writer is a handle of the library's), the fields read back are
the head as it went at the first flush (Go's `Result()`), and no `Date` or Content-Length is added: the framing is
the server's, and there is none.

## Rules

- A handler is called with `rec.writer()` as with any writer: `handler(req, rec.writer())`, a task's
  `.wait()` on a thread or `co_await` in a task.
- What a handler throws is not caught: it comes out of the call, or out of [serve](serve.md), to the test.
- A recorder is a handle of one word: a copy is the same recorder, and so is a copy of its writer.
- [hijack](../response_writer/hijack.md) of its writer is `std::errc::operation_not_supported`: there is no
  connection.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](response_recorder.md) | constructs an empty recorder |
| [writer](writer.md) | the writer to hand to a handler |
| [serve, async_serve](serve.md) | runs a request through a server's routes |

#### Observers

| Function | Description |
|---|---|
| [status](status.md) | the status written |
| [headers](headers.md) | the fields of the head |
| [header](header.md) | the first value of a field |
| [body](body.md) | everything written to the body |
| [trailers](trailers.md) | the trailers set |
| [informational](informational.md) | the informational responses sent |
| [flushes](flushes.md) | how many flushes the handler made |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

void greet(net::http::request r, net::http::response_writer w) {
    if (r.query("name").empty()) {
        w.error(net::http::status::bad_request, "a name, please");
        return;
    }
    w.set_header("Content-Type", "text/plain");
    w.write("hello, " + r.query("name") + "\n");
}

int main() {
    net::http::response_recorder ok;
    greet(net::http::test_request("GET", "/?name=Ada"), ok.writer());
    print("{} {} {}", ok.status(), ok.header("Content-Type"), ok.body());

    net::http::response_recorder bad;
    greet(net::http::test_request("GET", "/"), bad.writer());
    print("{} {}", bad.status(), bad.body());
}
```

Output:

```text
200 text/plain hello, Ada
400 a name, please
```

## See also

- [test_request](../test_request.md): the request a handler gets
- [test_server](../test_server/README.md): a handler served on the loopback
- [response_writer](../response_writer/README.md): what the handler writes to
