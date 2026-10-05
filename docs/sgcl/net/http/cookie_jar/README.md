[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::cookie_jar

```cpp
#include "sgcl/net/http/cookie_jar.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cookie_jar;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::cookie_jar` keeps the cookies the responses to a client set and gives each request the ones that
belong to it: Go's `net/http/cookiejar` with `x/net/publicsuffix` in one. A [client](../client/README.md) takes one in
its member `jar`, none by default as in Go; with one, every response's `Set-Cookie`, a redirect's among them, goes into
the jar, and every request carries the jar's cookies for its URL. The jar stands alone too: a program that speaks HTTP
some other way hands it what it received ([set_cookies](set_cookies.md)) and asks it what to send
([header](header.md), [cookies](cookies.md)).

It follows the storage model of RFC 6265 §5.3 and the rules of RFC 6265bis §5.7 that browsers follow: a cookie of a
domain only from a host under that domain and never for a public suffix, which the jar looks up in the
[Public Suffix List](../public_suffix.md) embedded in the library (`co.uk`, `github.io`); the default path from the
URL's; `Max-Age` before `Expires`, both held to 400 days; the `__Secure-` and `__Host-` prefixes; `Secure` set only
from a secure origin and never shadowed by a cookie from an insecure one. Go's jar has neither the prefixes nor the
last two rules, nor a way to keep its cookies across runs, which [save](save.md) and [load](load.md) give.

A jar is a handle of one word whose state is made in the constructor: copies are the same jar, and it is safe from
many threads at once, so one jar serves a client's concurrent requests, and several clients at once.

## Rules

- **Stored** (RFC 6265 §5.3, RFC 6265bis §5.7): a cookie with a `Domain` only from a host that is that domain or
  under it, a `Domain` equal to a public suffix only as the host's own cookie when the host is the suffix itself, an
  IP address's cookie its own alone; without a `Path` (or with one that does not begin with `/`), the directory of
  the URL's path; `Max-Age` wins over `Expires`, both held to 400 days, and zero, a negative age or a past date
  deletes the cookie of the same name, domain and path. A cookie of the same name, domain, host-only flag and path
  replaces the one there, which keeps its creation time.
- **Refused**: `Secure` from an origin that is not secure; a cookie without `Secure` from an insecure origin when a
  `Secure` cookie of its name is there for a domain and path it would shadow; a name beginning `__Secure-` without
  `Secure`, or `__Host-` without `Secure`, with a `Domain` or with a `Path` other than `/` (any case); `SameSite=None`
  without `Secure`; a name that is not a token, a name and value of more than 4096 bytes, a control in the value. A
  `Domain` or `Path` of more than 1024 bytes is ignored. A secure origin is `https` and `wss`, and the loopback as
  browsers take it: `localhost`, a name under it, `127.0.0.0/8` and `::1`.
- **Sent** (RFC 6265 §5.4): the cookies whose domain is the URL's host (a host-only cookie) or a domain it is under,
  whose path matches the URL's, `Secure` ones only to a secure origin, none expired: the longer paths first, then the
  older ones, their time of last use recorded. `SameSite` is kept and shown, and does not hold a cookie back: a client
  is no site of its own, so it has no cross-site request to tell apart.
- **Hosts** are compared as the [url](../../url/README.md) gives them, in lower case and in A-labels (IDNA), a dot
  at the end ignored; a `Domain` in Unicode is converted the same way. Schemes other than `http`, `https`, `ws` and
  `wss` set and get nothing.
- **Limits**: 180 cookies a registrable domain (the site, `example.co.uk`) and 3000 in all, by default
  ([options](../cookie_jar-options.md)). Past one, the expired go first, then within a domain those without `Secure`,
  the least recently used first in each.
- A moved-from jar is the same jar, as a moved-from [tracked_ptr](../../../core/tracked_ptr/README.md) still points.
  Nothing of it waits but [save](save.md) and [load](load.md); its other calls take a mutex of its own for a look
  over the cookies of one site.

## Member types

| Type | Definition |
|---|---|
| [options](../cookie_jar-options.md) | the limits of a jar |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cookie_jar.md) | an empty jar, with the default limits or the ones given; a copy |
| `(destructor)` | drops the handle; the jar stays while a copy holds it |
| `operator=` | makes this handle one of another's jar |

#### Cookies

| Function | Description |
|---|---|
| [set_cookies](set_cookies.md) | the cookies a response from a URL set, each stored, replaced, deleted or refused |
| [cookies](cookies.md) | the cookies a request to a URL carries, in their order |
| [header](header.md) | the `Cookie` field of a request to a URL |
| [all](all.md) | every cookie of the jar |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the cookies held |
| [empty](empty.md) | whether it holds none |

#### Modifiers

| Function | Description |
|---|---|
| [remove](remove.md) | a cookie by its domain, path and name, or every cookie of a domain |
| [clear](clear.md) | every cookie out |
| [clear_expired](clear_expired.md) | the expired cookies out |
| [clear_session](clear_session.md) | the session cookies out, as a browser's restart |

#### Persistence

| Function | Description |
|---|---|
| [save, async_save](save.md) | the jar written to a file, readable by its owner alone |
| [load, async_load](load.md) | the cookies of a file put in the jar |
| [to_json](to_json.md) | the jar as JSON text |
| [load_json](load_json.md) | the cookies of JSON text put in the jar |

## Example

A server that sets a cookie on a redirect and reads it after, and a client with a jar:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /login", [](net::http::request, net::http::response_writer w) {
        w.add_cookie(net::http::cookie("session=s3cr3t; Path=/; HttpOnly"));
        w.redirect("/home");
    });
    srv.route("GET /home", [](net::http::request req, net::http::response_writer w) {
        w.write("session " + req.cookie("session") + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    web.jar = net::http::cookie_jar();
    print("{}", web.get(base + "/login")->text().value());
    print("{}", web.get(base + "/home")->text().value());
    println("{}", web.jar->header(net::url(base + "/")));
    srv.close();
}
```

Output:

```text
session s3cr3t
session s3cr3t
session=s3cr3t
```

## See also

- [cookie](../cookie/README.md): a cookie, the `Set-Cookie` field written and read
- [client](../client/README.md): the member `jar`
- [public_suffix](../public_suffix.md), [registrable_domain](../registrable_domain.md): the list the jar stands on
- [response::cookies](../response/cookies.md): the cookies a response set
- `tests/net/http/cookie_jar.cpp` (the RFCs' rules one by one, limits, persistence, threads),
  `tests/net/http/cookie_jar_client.cpp` (a client with a jar against the module's server: redirects, HTTP/2,
  WebSocket), `tests/net/http/cookie_jar_go.cpp` (Go's `net/http/cookiejar` as the oracle)
- [sgcl::net::http](../README.md)
