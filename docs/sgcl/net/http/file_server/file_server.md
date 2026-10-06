[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [file_server](README.md)

# sgcl::net::http::file_server::file_server

```cpp
explicit file_server(const string& directory, const serve_options& o = {}) noexcept;
```

Makes the handler of the files of `directory`, answered by the options: the ETag made for each file and whether ranges
are answered. The directory is not opened or checked here: a directory that is not there makes every request 404, as
a file that is not there does.

## Parameters

| Parameter | Description |
|---|---|
| `directory` | the directory whose files are served |
| `o` | the ETag and the ranges ([serve_options](../serve_options.md)); a weak ETag and ranges by default |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("files");
    (void)io::write_file("files/a.txt", "the file a\n");
    net::http::file_server files("files", {.etag = net::http::etag_kind::strong});
    net::http::server srv;
    srv.route("GET /files/{path...}", files);
    auto served = srv.serve(":8080");
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl http://localhost:8080/files/a.txt
the file a
$ curl -s -o /dev/null -w '%{http_code}\n' http://localhost:8080/files/none.txt
404
```

## See also

- [operator()](operator_call.md): the handler
- [file_server](README.md)
