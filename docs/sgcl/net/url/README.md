[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::url

```cpp
#include "sgcl/net/url.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class url;
}
```

`sgcl::net::url` is a URL read the way a browser reads one, by the [WHATWG URL
Standard](https://url.spec.whatwg.org/), as curl, Node and Deno read it. The whole of the standard's test data from
the web platform tests is the oracle, and all of it passes: `urltestdata.json` (every input with and without a base,
and every part after it), `setters_tests.json` (every setter) and `toascii.json` (the host parser). The host goes
through IDNA ([txt::idna](../../txt/idna/README.md), with the standard's profile), and the parts are escaped with the standard's
sets ([txt::percent](../../txt/percent/README.md)). `#include "sgcl/net/url.h"` alone brings the URL without the reactor or
sockets: with it come only core, the [address types](../ip_address/README.md), [io::error](../../io/error/README.md) and
txt's IDNA, escaping and [format](../../txt/format.md).

A `url` is a value, as a [string](../../core/string/README.md) is: one string, its serialization, and the places of the parts in
it, 48 bytes. It is made only by the parser ([parse](parse.md), the [constructor](url.md)) or by a setter of
the standard (`with_*`), so every `url` is a URL: there is no empty one and no field to set to something that is not a
URL, as there is in Go's `url.URL`. A part is read from the serialization as it stands, and a setter returns a new
`url`.

Go's `net/url` reads RFC 3986, loosely, and differs where the standard follows the browsers: a `\` in a special scheme
is a `/`; tabs and newlines vanish; the host goes through IDNA (`bücher.de` is `xn--bcher-kva.de`) and is lowercased;
an IPv4 host may be written `0x7f.1` or `2130706433`; the path is resolved (`/a/../b` is `/b`); a space is escaped; a
special URL with an empty path has the path `/` (`http://g` is `http://g/`); the port of the scheme (`:80` for http)
is dropped.

## Rules

- **A url holds a string**, so it lives where a [string](../../core/string/README.md) may: on a stack, in a task, in a managed
  object; in a global or a `std` container, a [rooted](../../core/rooted/README.md) of it ([The
  rules](../../core/README.md#the-rules), 1). It is immutable: threads share one as they share a string.
- **Special schemes.** `http`, `https`, `ws`, `wss`, `ftp` and `file` are read as the standard's special schemes: a
  host is required (none for file), `\` is `/`, the default port is dropped, the path is hierarchical. Any other
  scheme keeps its host opaque (`sc://1.2.3.4/` has the host `1.2.3.4`, not an address) and may have an opaque path
  (`mailto:x@example.com`: [has_opaque_path](has_opaque_path.md), the path is everything after the `:`).
- **The parts are escaped.** [path](path.md), [query](query.md), [fragment](fragment.md),
  [username](username.md) and [password](password.md) return the parts as the URL writes them: `path()` of
  `http://x/a b` is `/a%20b`. A query's values are read unescaped by [query_params](query_params.md).
- **The host.** [host](host.md) is the host and the port when one is written, as the standard's `host` getter and
  Go's `URL.Host` have it; [hostname](hostname.md) is the host alone, without the port and without the brackets of
  an IPv6 address, as Go's `Hostname()`; [host_address](host_address.md) is the IP address of a host that is one.
  An IPv6 host is written as the standard writes it, the first longest run of zero pieces as `::`, with no dotted tail
  (`[::ffff:102:304]`, where RFC 5952 writes `::ffff:1.2.3.4`).
- **The port.** [port](port.md) is the port written, `nullopt` when none was and when the one written is the
  scheme's default; [effective_port](effective_port.md) is the port or the default.
- **The setters** are the standard's (§6.1), each returning a new `url` or the error `net::errc::invalid_url`, with
  the setter and the value asked for (never a password), where the standard refuses the value or declines to apply it,
  and past the limit below, the one refusal of [with_query](with_query.md) and [with_fragment](with_fragment.md);
  [without_fragment](without_fragment.md) cannot fail. A host is carried from one URL to another by
  [host](host.md) and [with_host](with_host.md), which take and give the same form.
- **Relative references** are resolved by the same parser, given a base: [parse](parse.md) with a base, or
  [resolve](resolve.md), and for a reference the program itself writes the [constructor](url.md) with a base,
  which throws `parse`'s error. The examples of RFC 3986 §5.4 resolve as Go resolves them, but for `//g` (see above).
- **The limit.** A URL's text is at most 512 MiB as the parser reads it, and so is the URL made of it:
  `net::detail::UrlMaxSize` in `sgcl/net/url.h`. [parse](parse.md), [resolve](resolve.md) and the setters
  give `net::errc::invalid_url` for a text past it and for one whose URL would pass it (the parser drops tabs and
  newlines and, for parse, the spaces at the ends; a byte that is not UTF-8 is read as the three of U+FFFD, an escaped
  byte is written as three), and for a host that goes through IDNA longer than 1 MiB once
  decoded (`net::detail::UrlMaxIdnaSize`): what IDNA makes of that stays under 2 GiB. Three times the limit is
  below the 4 GiB of a [string](../../core/string/README.md), so none of them throws. A real URL is far shorter: browsers stop
  near 2 MB. [query_params](../query_params/README.md#rules) keeps the same limit.
- **Text that is not UTF-8.** The standard's input is code points; a byte that does not begin a valid UTF-8 sequence
  is taken as U+FFFD, which a path escapes and a host refuses. Tabs and newlines anywhere, and C0 controls and spaces
  at the ends, are removed before parsing.
- **Equality and order** are those of the serialization: `HTTP://EXAMPLE.com:80/./a` equals `http://example.com/a`.
- **Errors are values.** A text that is not a URL, and a value a setter refuses, are an [io::error](../../io/error/README.md) of
  the code `net::errc::invalid_url` ([errc](../errc.md)); only a literal the [constructor](url.md) cannot read
  throws it.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](url.md) | constructs the URL a literal spells, absolute or relative to a base |

#### Parsing

| Function | Description |
|---|---|
| [parse](parse.md) | reads a URL, or a reference relative to a base (static) |
| [resolve](resolve.md) | reads a reference relative to this URL |

#### Parts

| Function | Description |
|---|---|
| [scheme](scheme.md) | the scheme, `https` |
| [username](username.md) | the user name, escaped |
| [password](password.md) | the password, escaped |
| [host](host.md) | the host and the port when one is written |
| [hostname](hostname.md) | the host alone, without the port and the brackets |
| [has_host](has_host.md) | checks whether the URL has a host |
| [host_address](host_address.md) | the host when it is an IP address |
| [port](port.md) | the port written, if it is not the scheme's default |
| [effective_port](effective_port.md) | the port, or the scheme's default |
| [path](path.md) | the path, escaped |
| [query](query.md) | the query without its `?`, escaped |
| [fragment](fragment.md) | the fragment without its `#` |
| [has_query](has_query.md) | checks whether the URL has a query, an empty one among them |
| [has_fragment](has_fragment.md) | checks whether the URL has a fragment, an empty one among them |
| [has_opaque_path](has_opaque_path.md) | checks whether the path is opaque, `mailto:x` |
| [is_special](is_special.md) | checks whether the scheme is one of the standard's special ones |
| [origin](origin.md) | the origin, `https://example.com:8443`, or `null` |
| [request_target](request_target.md) | the path and the query, what an HTTP request line carries |
| [query_params](query_params.md) | the query's pairs, unescaped |

#### New versions

| Function | Description |
|---|---|
| [with_scheme](with_scheme.md) | the URL with another scheme |
| [with_username](with_username.md) | the URL with another user name |
| [with_password](with_password.md) | the URL with another password |
| [with_host](with_host.md) | the URL with another host, and a port when one is given |
| [with_hostname](with_hostname.md) | the URL with another host, the port kept |
| [with_port](with_port.md) | the URL with another port, or without one |
| [with_path](with_path.md) | the URL with another path |
| [with_query](with_query.md) | the URL with another query, or without one |
| [with_fragment](with_fragment.md) | the URL with another fragment, or without one |
| [without_fragment](without_fragment.md) | the URL without its fragment |

#### Text

| Function | Description |
|---|---|
| [to_string](to_string.md) | the serialization, `href` |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare two URLs by their serializations |
| [format_value](format_value.md) | writes the URL for [txt::format](../../txt/format.md) and `println`, `{}` its `to_string()` |

## Specializations

```cpp
namespace std {
    template<> struct hash<sgcl::net::url>;
}

template<>
struct sgcl::txt::formatter<sgcl::net::url>;
```

The hash of the serialization, the [string](../../core/string/README.md)'s, `noexcept`: a URL is a key of a
[map](../../core/map/README.md) or a [set](../../core/set/README.md) as it is, and two URLs equal under `==` hash alike.
The formatter tells [txt::format](../../txt/format.md) which specifications a URL takes — the width, the fill
and the alignment, no type and no precision — so that a literal pattern is checked where it is compiled; the
writing is [format_value](format_value.md)'s.

## Complexity

[parse](parse.md), the constructors and the setters are linear in the length of the text read and of the URL; a
part is a substring of the serialization, linear in its length; the predicates and [port](port.md) are constant.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("HTTPS://B\xC3\xBC" "cher.de/a/../suche?q=katze&page=2#treffer");
    println("{}", u);
    println("{} {} {}", u.host(), u.path(), u.effective_port());
    println(u.query_params().get("q"));

    auto params = u.query_params();
    params.set("page", "3");
    println("{}", *u.with_query(params));
    auto moved = u.with_port(8443)->without_fragment();
    println("{} {}", moved.origin(), moved.request_target());

    net::url logo("/img/logo.png", u);
    println("{}", logo);
    println(net::url::parse("/relative") ? "parsed" : "not a URL without a base");
}
```

Output:

```text
https://xn--bcher-kva.de/suche?q=katze&page=2#treffer
xn--bcher-kva.de /suche 443
katze
https://xn--bcher-kva.de/suche?q=katze&page=3#treffer
https://xn--bcher-kva.de:8443 /suche?q=katze&page=2
https://xn--bcher-kva.de/img/logo.png
not a URL without a base
```

## See also

- [query_params](../query_params/README.md): a query's pairs
- [txt::idna](../../txt/idna/README.md) and [txt::percent](../../txt/percent/README.md): the host's IDNA and the escaping, which a URL
  uses with the standard's profile and sets
- [http](../http/README.md): where a `url` is sent; [http::request](../http/request/README.md) parses its URL with this parser
- `tools/url_oracle.go` (the oracle: `go run tools/url_oracle.go ~/Programming/oracles/wpt-url >
  tests/net/url_oracle.h`, from the web platform tests' `url/resources`), `tests/net/url.cpp`
