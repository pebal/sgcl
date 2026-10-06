[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cache](README.md)

# sgcl::net::http::cache::on_disk

```cpp
static expected<cache, io::error> on_disk(const string& directory);                      // (1)
static expected<cache, io::error> on_disk(const string& directory, const options& o);    // (2)
```

A cache in a directory, made with its parents when it is missing: each response two files, its head and its body,
named after the URL and its `Vary` values, written whole and renamed into place. The entries a cache of the directory
kept before are taken at once; a file that does not read back — a write the program's end broke off, a stranger — is
removed. One cache of a directory at a time: two programs writing to one directory do not see each other's entries
until they open it again.

1. With the default limits.
2. With the [options](../cache-options.md) given.

## Parameters

| Parameter | Description |
|---|---|
| `directory` | where the files are kept |
| `o` | the limits and the heuristic |

## Return value

The cache, or the error of making the directory or of reading it.

## Complexity

Linear in the number of entries the directory holds.

## Exceptions

None: a failure is in the returned error.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

#include <atomic>

using namespace sgcl;

int main() {
    std::atomic<int> asked{0};
    net::http::server srv;
    srv.route("GET /news", [&asked](net::http::request, net::http::response_writer w) {
        ++asked;
        w.set_header("Cache-Control", "max-age=60");
        w.write("news " + to_string(asked.load()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    {
        net::http::client web;
        web.cache = net::http::cache::on_disk("http-cache").value();
        println("{}", web.get(url)->text().value());
    }
    net::http::client later;  // another run of the program, as it were
    later.cache = net::http::cache::on_disk("http-cache").value();
    auto res = later.get(url);
    println("{} {}", res->text().value(), later.cache->size());
    println("the server asked {} time(s)", asked.load());
    srv.close();
}
```

Output:

```text
news 1
news 1 1
the server asked 1 time(s)
```

## See also

- [(constructor)](cache.md)
- [cache](README.md)
