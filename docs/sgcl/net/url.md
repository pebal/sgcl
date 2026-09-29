# sgcl::net::url, sgcl::net::query_params

```cpp
#include "sgcl/net/url.h"   // or "sgcl/net/net.h"

namespace sgcl::net {
    class url;              // a URL by the WHATWG URL Standard: immutable, one string and the places of its parts
    class query_params;     // application/x-www-form-urlencoded: name and value pairs in their order
}
// std::hash<net::url>
```

A URL read the way a browser reads one, by the [WHATWG URL Standard](https://url.spec.whatwg.org/), as curl, Node and Deno do: the whole of the standard's test data from the web platform tests (`urltestdata.json`, every input with and without a base and every part after it; `setters_tests.json`, every setter; `toascii.json`, the host parser) is the oracle, and all of it passes. The host goes through IDNA ([`txt::idna`](../txt/idna.md), the standard's profile), the parts are escaped with the standard's sets ([`txt::percent`](../txt/percent.md)).

A `url` is a value, as a [`string`](../core/string.md) is: one string, its serialization, and the places of the parts in it. It is made only by the parser or by a setter of the standard (`with_*`), so every `url` is a URL: there is no field to set to something that is not one, as there is in Go's `url.URL`.

Go's `net/url` reads RFC 3986, loosely, and differs where the standard follows the browsers: a `\` in a special scheme is a `/`; tabs and newlines vanish; the host goes through IDNA (`bücher.de` is `xn--bcher-kva.de`) and is lowercased; an IPv4 host may be written `0x7f.1` or `2130706433`; the path is resolved (`/a/../b` is `/b`); a space is escaped; a special URL with an empty path has the path `/` (`http://g` is `http://g/`); the port of the scheme (`:80` for http) is dropped.

## Rules

- **Special schemes.** `http`, `https`, `ws`, `wss`, `ftp` and `file` are read as the standard's special schemes: a host is required (none for file), `\` is `/`, the default port is dropped, the path is hierarchical. Any other scheme keeps its host opaque (`sc://1.2.3.4/` has the host `1.2.3.4`, not an address) and may have an opaque path (`mailto:x@example.com`: `has_opaque_path()`, the path is everything after the `:`).
- **The parts are escaped.** `path()`, `query()`, `fragment()`, `username()` and `password()` return the parts as the URL writes them: `path()` of `http://x/a b` is `/a%20b`. To read a query's values unescaped, `query_params()`.
- **The host.** `host()` is the host and the port when one is written, as the standard's `host` getter and Go's `URL.Host` have it: `example.com:8443`, `[::1]:8080`, `xn--bcher-kva.de`, `10.0.0.1`; `hostname()` is the host alone, without the port and without the brackets of an IPv6 address, as Go's `Hostname()` (the standard's `hostname` getter keeps the brackets). `host_address()` gives the IP address of a host that is one. An IPv6 host is written as the standard writes it, the first longest run of zero pieces as `::`, with no dotted tail (`[::ffff:102:304]`, where RFC 5952 writes `::ffff:1.2.3.4`).
- **The port.** `port()` is the port written, `nullopt` when none was and when the one written is the scheme's default; `effective_port()` is the port or the default (80, 443, 21; 0 for a scheme without one).
- **The setters** are the standard's (§6.1), each returning a new `url`. `with_scheme`, `with_username`, `with_password`, `with_host`, `with_hostname`, `with_port` and `with_path` return the error `net::errc::invalid_url`, with the setter and the value asked for (never a password), where the standard refuses the value or declines to apply it: a scheme that is not one; a special scheme for one that is not, or the other way; a host that does not parse; a host given with a port to `with_hostname` (`with_host` is the standard's host setter and takes one, so that what `host()` gives `with_host` takes; `with_hostname` is the standard's hostname setter and takes an IPv6 address in its brackets, `[::1]`, where `hostname()` gives it without them, as Go's `Hostname()` does, so a host is carried from one URL to another by `host()` and `with_host`); a host followed by `:` and nothing (`with_host("a:")`) keeps the port, as the standard's setter does; credentials or a port for a URL without a host or with file's; a host or a path for a URL with an opaque path. `with_query` and `with_fragment` always apply: `""` removes the part, a leading `?` or `#` is dropped.
- **Relative references** are resolved by the same parser, given a base: `parse(reference, base)`, or `base.resolve(reference)`, and for a reference the program itself writes `net::url(reference, base)`, which throws `parse`'s error. The examples of RFC 3986 §5.4 resolve as Go resolves them, but for `//g` (see above).
- **Text that is not UTF-8.** The standard's input is code points; a byte that does not begin a valid UTF-8 sequence is taken as U+FFFD, which a path escapes and a host refuses. Tabs and newlines anywhere, and C0 controls and spaces at the ends, are removed before parsing.
- **Equality and order** are those of the serialization: `HTTP://EXAMPLE.com:80/./a` equals `http://example.com/a`.
- **`query_params`** is a list, not a map: a name may come many times, the order is kept, `get` walks the list (a query is a handful of pairs, and nothing on the network can steer a hash). `parse` never fails, as in the standard: `+` is a space, a broken escape stays as it is written, bytes that are not UTF-8 after unescaping become U+FFFD. `to_string` writes a space as `+` and escapes everything but the letters, the digits and `* - . _`.

## Members

### url: parsing

```cpp
static expected<url, io::error> parse(const string& text);                   // an absolute URL, else errc::invalid_url
static expected<url, io::error> parse(const string& text, const url& base);  // absolute, or relative to base
explicit url(const string& text);                                          // a literal: parse(text), absolute, or bad_expected_access<io::error> (DESIGN 234)
explicit url(const string& text, const url& base);                         // a literal reference: parse(text, base), or the same
expected<url, io::error> resolve(const string& reference) const;           // parse(reference, *this)
```

`parse` is for text from outside, which may not be a URL; the constructors are for a URL the program writes itself, and throw where `parse` fails. A reference is resolved against a base by the same parser.

### url: the parts

```cpp
string scheme() const;          // "https", without the ':'
string username() const;        // escaped
string password() const;
string host() const;            // "example.com:8443", "[::1]:8080", "10.0.0.1"; "" when none
string hostname() const;        // "example.com", "::1": no port, no brackets
bool has_host() const noexcept;
optional<ip_address> host_address() const;   // for an IPv4 or IPv6 host
optional<uint16_t> port() const noexcept;    // nullopt: none, or the scheme's default
uint16_t effective_port() const noexcept;    // the port or 80, 443, 21; 0 for none
string path() const;            // "/a/b%20c"; the opaque path of "mailto:x"
string query() const;           // without '?', escaped; "" when none
string fragment() const;        // without '#'
bool has_query() const noexcept;
bool has_fragment() const noexcept;
bool has_opaque_path() const noexcept;
bool is_special() const;        // http, https, ws, wss, ftp, file
string origin() const;          // "https://example.com:8443"; "null" for other schemes, file among them
string request_target() const;  // the path and the query: what an HTTP request line carries
net::query_params query_params() const;
```

Each part as the URL writes it, escaped; the host with its port and without it, the port written and the port in effect, and the query's pairs unescaped by `query_params()`.

### url: the setters

```cpp
expected<url, io::error> with_scheme(const string& scheme) const;
expected<url, io::error> with_username(const string& username) const;
expected<url, io::error> with_password(const string& password) const;
expected<url, io::error> with_host(const string& host) const;          // the standard's host setter: "example.com:8080" sets both, host()'s pair
expected<url, io::error> with_hostname(const string& hostname) const;  // the hostname setter: the host alone
expected<url, io::error> with_port(optional<uint16_t> port) const;  // nullopt removes it
expected<url, io::error> with_path(const string& path) const;
url with_query(const string& query) const;              // "" removes it
url with_query(const net::query_params& params) const;  // no pairs: no query
url with_fragment(const string& fragment) const;        // "" removes it
url without_fragment() const;
```

The standard's setters, each returning a new `url`. Those that may refuse the value return `net::errc::invalid_url`; the query and the fragment always apply.

### url: text and comparison

```cpp
string to_string() const;       // the serialization (href)
friend bool operator==(const url&, const url&) noexcept;
friend std::strong_ordering operator<=>(const url&, const url&) noexcept;
```

The serialization, and equality and order by it.

### query_params: parsing and reading

```cpp
query_params();
static query_params parse(const string& text);   // "a=1&b=x+y"; a leading '?' dropped; never fails
explicit query_params(const string& text);        // a literal: the same as parse(text), nothing to throw (DESIGN 234)
string get(const string& name) const;             // the first value, or ""
vector<string> get_all(const string& name) const;
bool contains(const string& name) const;
```

A query read as the standard reads one, which never fails; a name's first value, all of them, or whether it is there.

### query_params: writing

```cpp
query_params& add(const string& name, const string& value);   // at the end
query_params& set(const string& name, const string& value);   // the first takes it, the others go
query_params& erase(const string& name);                       // every pair of the name
```

A pair added at the end, a name's value set in the place of its first pair, or every pair of a name dropped.

### query_params: iteration and text

```cpp
size_t size() const noexcept;
bool empty() const noexcept;
auto begin() const noexcept;    // pair<string, string>, in order
auto end() const noexcept;
string to_string() const;       // "a=1&b=x+y"
bool operator==(const query_params&) const;
```

The pairs in their order, and the query written back, a space as `+`.

## Examples

### The parts

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/net.h"

using namespace sgcl;

int main() {
    // a literal: the constructor
    net::url u("HTTPS://B\xC3\xBC" "cher.de/a/../suche?q=katze&page=2#treffer");
    println(u.to_string());
    println("{} {} {}", u.host(), u.path(), u.effective_port());
    println(u.query_params().get("q"));
}
```

Output:

```text
https://xn--bcher-kva.de/suche?q=katze&page=2#treffer
xn--bcher-kva.de /suche 443
katze
```

### The setters

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/net.h"

using namespace sgcl;

int main() {
    net::url u("HTTPS://B\xC3\xBC" "cher.de/a/../suche?q=katze&page=2#treffer");
    auto next = u.with_query(u.query_params().set("page", "3"));
    println(next.to_string());

    auto moved = u.with_port(8443)->without_fragment();
    println("{} {}", moved.origin(), moved.request_target());
}
```

Output:

```text
https://xn--bcher-kva.de/suche?q=katze&page=3#treffer
https://xn--bcher-kva.de:8443 /suche?q=katze&page=2
```

### References

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/net.h"

using namespace sgcl;

int main() {
    net::url u("HTTPS://B\xC3\xBC" "cher.de/a/../suche?q=katze&page=2#treffer");
    net::url logo("/img/logo.png", u);  // a literal reference against a base
    println(logo.to_string());

    // text that may fail: parse
    println(net::url::parse("/relative") ? "parsed" : "not a URL without a base");
}
```

Output:

```text
https://xn--bcher-kva.de/img/logo.png
not a URL without a base
```

## See also

- [txt idna](../txt/idna.md) and [txt percent](../txt/percent.md): the host's IDNA and the escaping, which this page uses with the standard's profile and sets
- [http](http/README.md): where a `url` is sent
- `tools/url_oracle.go` (the oracle: `go run tools/url_oracle.go ~/Programming/oracles/wpt-url > tests/net/url_oracle.h`, from the web platform tests' `url/resources`), `tests/net/url.cpp`
