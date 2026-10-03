[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::request

```cpp
request(const string& method, const string& url) noexcept;
```

Constructs a request to send, Go's `http.NewRequest`: the method as given (`"GET"`, `"POST"`, a method of the
program's own), the URL parsed now by [net::url](../../url.md). A URL that does not parse is not an error here: the
send reports it, `net::errc::invalid_url`, and [url](url.md) throws for it; so does a text past 512 MiB ([the
limit](../../url.md#rules)). A method that is not a token is refused by
the send in the same way, before a connection is dialed. The request has no fields and no body until the program sets
them. The copy constructor is the implicit one: a copy is the same request.

## Parameters

| Parameter | Description |
|---|---|
| `method` | the method, sent as given |
| `url` | the URL, `http://` or `https://` |

## Complexity

Linear in the size of `url`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::request note("PATCH", "https://example.com/notes/7?draft=1");
    println("{} {} {}", note.method(), note.url().hostname(), note.url().path());

    net::http::request broken("GET", "no URL at all");
    net::http::client web;
    println("{}", web.send(broken).error().message());
}
```

Output:

```text
PATCH example.com /notes/7
GET no URL at all: invalid URL
```

## See also

- [set_header](set_header.md), [set_body](set_body.md): what goes with it
- [send](../client/send.md): the request sent
- [sgcl::net::http::request](../request.md)
