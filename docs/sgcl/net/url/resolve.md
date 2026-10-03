[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::resolve

```cpp
expected<url, io::error> resolve(const string& reference) const noexcept;
```

The reference resolved against this URL: [parse](parse.md)`(reference, *this)`, Go's `URL.Parse` of a reference. The
examples of RFC 3986 §5.4 resolve as Go resolves them, but for `//g`, whose empty path is `/` in a special scheme.

## Parameters

| Parameter | Description |
|---|---|
| `reference` | the reference: a URL, an absolute or relative path, a query, a fragment |

## Return value

The URL, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` when the reference does not
resolve, is longer than 512 MiB, or would make a URL longer than that ([the limit](README.md#rules)).

## Complexity

Linear in the length of `reference` and of this URL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url base("http://a/b/c/d;p?q");
    for (const char* ref : {"g", "./g", "g/", "/g", "//g", "?y", "g?y", "#s", "../g", "../../../g", ""}) {
        println("\"{}\" -> {}", ref, base.resolve(ref)->to_string());
    }
}
```

Output:

```text
"g" -> http://a/b/c/g
"./g" -> http://a/b/c/g
"g/" -> http://a/b/c/g/
"/g" -> http://a/g
"//g" -> http://g/
"?y" -> http://a/b/c/d;p?y
"g?y" -> http://a/b/c/g?y
"#s" -> http://a/b/c/d;p?q#s
"../g" -> http://a/b/g
"../../../g" -> http://a/g
"" -> http://a/b/c/d;p?q
```

## See also

- [parse](parse.md): the same with the base given
- [(constructor)](url.md): a literal reference
- [sgcl::net::url](README.md)
