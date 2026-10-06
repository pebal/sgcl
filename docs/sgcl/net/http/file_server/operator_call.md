[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [file_server](README.md)

# sgcl::net::http::file_server::operator()

```cpp
void operator()(request req, response_writer w) const;
```

Answers a request with a file of the directory: what a [server](../server/README.md) calls with each request routed
to it. The name is the route's `{path...}` value, or the URL's path unescaped when the route has no such wildcard; it
goes through [io::path::under](../../../io/path/under.md), and a name that would leave the directory is 404. A
directory is its `index.html`, its URL without the slash redirected (301) to the one with it. The file is answered as
[serve_file](../serve_file.md) answers it.

A plain handler: it never waits. A [response_recorder](../response_recorder/README.md)'s writer takes it as a server's
does.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request received |
| `w` | its writer |

## Return value

None.

## Complexity

That of [serve_file](../serve_file.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("site");
    (void)io::write_file("site/index.html", "<h1>home</h1>\n");
    net::http::file_server files("site");
    net::http::response_recorder page, missing;
    files(net::http::test_request("GET", "/index.html"), page.writer());
    files(net::http::test_request("GET", "/../secret.txt"), missing.writer());
    print("{} {}", page.status(), page.body());
    println("{}", missing.status());
}
```

Output:

```text
200 <h1>home</h1>
404
```

## See also

- [file_server](README.md)
