[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::json, async_json

```cpp
/*(1)*/ expected<encoding::json, io::error> json() const;
/*(2)*/ async::task<expected<encoding::json, io::error>> async_json() const noexcept;
/*(3)*/ template<class T>
        expected<T, io::error> json() const;
/*(4)*/ template<class T>
        async::task<expected<T, io::error>> async_json() const noexcept;
```

Reads the whole body as [text](text.md) and parses it as JSON, Go's `json.NewDecoder(resp.Body).Decode(&v)`. The
`Content-Type` of the response is not looked at. The end of the body gives the connection back to the client's pool.

1. The body as an [encoding::json](../../../encoding/json.md) value. Blocks the calling thread: the reading runs on
   the scheduler and the thread waits for it.
2. The same, as a task.
3. The body as a `T` of the program's, filled through its `describe` ([encoding::json::parse](../../../encoding/json.md)
   of `T`). Blocks the calling thread.
4. The same, as a task.

- (1), (3) For a thread of the program, never a worker; a task writes `co_await res.async_json()`.

## Parameters

None.

## Return value

The value, or the error: that of the reading, as for [text](text.md), or the error of the JSON, an
[io::error](../../../io/error.md) of the operation `decode` on `json` (a text that is not JSON; for `T`, a member of
the wrong kind).

## Complexity

Linear in the size of the body.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

struct note {
    string text;
    bool done = false;

    void describe(encoding::field_list& f) {
        f.add("text", text);
        f.add("done", done);
    }
};

int main() {
    net::http::server srv;
    srv.route("GET /note", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "application/json");
        w.write("{\"text\": \"buy milk\", \"done\": true, \"tags\": [\"home\"]}");
    });
    srv.route("GET /broken", [](net::http::request, net::http::response_writer w) {
        w.write("{\"text\": ");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    note n = web.get(base + "/note")->json<note>();
    println("{} {}", n.text, n.done);
    encoding::json value = web.get(base + "/note")->json();
    println("{}", value["text"].as_string("?"));
    auto broken = web.get(base + "/broken")->json();
    println("{} {}", broken.error().op(), broken.error().path());
    srv.close();
}
```

Output:

```text
buy milk true
buy milk
decode json
```

## See also

- [text](text.md): the body as text
- [encoding::json](../../../encoding/json.md): the value and `describe`
- [sgcl::net::http::response](../response.md)
