[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::parse

```cpp
/*(1)*/ static expected<url, io::error> parse(const string& text) noexcept;
/*(2)*/ static expected<url, io::error> parse(const string& text, const url& base) noexcept;
```

Reads a URL by the WHATWG URL Standard, as a browser reads one; Go's `url.Parse`.

1. An absolute URL: `"https://user@example.com:8443/a/b?q=1#top"`. Anything else, a relative reference among it, is an
   error.
2. A URL, or a reference relative to `base`: `"../c"`, `"?q=2"`, `"//host/x"`, resolved by the same parser; Go's
   `URL.Parse` of a reference, or `ResolveReference`.

Before parsing, tabs and newlines anywhere, and C0 controls and spaces at the ends, are removed. The standard's input
is code points: a byte that does not begin a valid UTF-8 sequence is taken as U+FFFD, which a path escapes and a host
refuses. The scheme is lowercased and a hierarchical path resolved (`/a/../b` is `/b`); in a special scheme (`http`,
`https`, `ws`, `wss`, `ftp`, `file`) a `\` is a `/`, the host goes through IDNA and is lowercased, an IPv4 host may be
written in the browsers' forms (`0x7f.1`, `2130706433`), an empty path is `/`, and the scheme's default port is
dropped. Go's `net/url` reads RFC 3986, loosely, and of all this only lowercases the scheme.

The text as the parser reads it (its ends trimmed, its tabs and newlines gone, a byte that is not UTF-8 counted as the
three of U+FFFD) is at most 512 MiB, and so is the URL made of it, an escaped byte counted as three; a host that goes
through IDNA is at most 1 MiB once decoded ([the limit](../url.md#rules)). A real URL is far shorter: browsers stop
near 2 MB.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |
| `base` | the URL a relative reference is resolved against |

## Return value

The URL, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the operation
`parse URL` and the text: its message reads `parse URL /relative: invalid URL`. A text past the limit is the same
error.

## Complexity

Linear in the length of `text` and of `base`.

## Exceptions

None.

## Notes

A text from outside the program is parsed; a URL the program itself writes is constructed,
`net::url("https://example.com/")` ([constructor](url.md)), and a wrong one throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"HTTP://Example.COM:80/a/./b/../c", "http:\\\\example.com\\a b",
                             "https://b\xC3\xBC" "cher.de/", "http://0x7f.1/", "mailto:x@example.com",
                             "/relative", "http://exa mple.com/"}) {
        auto u = net::url::parse(text);
        println(u ? u->to_string() : u.error().message());
    }
    net::url base("https://example.com/a/b");
    println(net::url::parse("../c?x=1", base)->to_string());
}
```

Output:

```text
http://example.com/a/c
http://example.com/a%20b
https://xn--bcher-kva.de/
http://127.0.0.1/
mailto:x@example.com
parse URL /relative: invalid URL
parse URL http://exa mple.com/: invalid URL
https://example.com/c?x=1
```

## See also

- [(constructor)](url.md): the URL a literal spells
- [resolve](resolve.md): a reference against this URL
- [to_string](to_string.md): the serialization
- [sgcl::net::url](../url.md)
