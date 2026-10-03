[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::url

```cpp
explicit url(const string& text);                     // (1)
explicit url(const string& text, const url& base);    // (2)
```

Constructs the URL a literal in the program spells.

1. An absolute URL: what [parse](parse.md) reads, or `bad_expected_access<io::error>` with `parse`'s error.
2. A URL or a reference relative to `base` (`"../c"`, `"?q=2"`, `"//host/x"`): what `parse(text, base)` reads, or the
   same exception.

A text from outside the program (a request, a setting, a page) may not be a URL: it is parsed, and its error is a
value. There is no default constructor: every `url` is a URL.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the URL, or the reference to resolve against `base` |
| `base` | the URL a relative reference is resolved against |

## Complexity

Linear in the length of `text` and of `base`.

## Exceptions

`bad_expected_access<io::error>` when `text` is not a URL (1), or not a reference that resolves against `base` (2),
or is past [the limit](../url.md#rules) of 512 MiB; its `error()` is `parse`'s, of the code `net::errc::invalid_url`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url home("https://example.com/docs/");
    net::url page("guide/intro.html", home);
    println("{} {}", home, page);

    try {
        net::url relative("/docs");
    } catch (const bad_expected_access<io::error>& e) {
        println(e.error().message());
    }
}
```

Output:

```text
https://example.com/docs/ https://example.com/docs/guide/intro.html
parse URL /docs: invalid URL
```

## See also

- [parse](parse.md): reads a text from outside the program
- [resolve](resolve.md): a reference against this URL
- [sgcl::net::url](../url.md)
