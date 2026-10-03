[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie](README.md)

# sgcl::net::http::cookie::cookie

```cpp
cookie() noexcept = default;                                 // (1)
cookie(const string& name, const string& value) noexcept;    // (2)
explicit cookie(const string& field);                        // (3)
```

Constructs a cookie.

1. An empty cookie: no name, no value, no attribute.
2. A cookie of `name` and `value`, without attributes; they are set as fields after.
3. The cookie a `Set-Cookie` literal of the program spells, `parse(field).value()`: input from outside is read by
   [parse](parse.md), which returns its error; a text the program itself wrote is constructed.

The copy and the move constructors are the implicit ones.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the cookie, checked when it is written |
| `value` | its value |
| `field` | the value of a `Set-Cookie` field: `name=value; Attribute=...` |

## Complexity

- (1) Constant.
- (2) Linear in the sizes of `name` and `value`.
- (3) Linear in the size of `field`.

## Exceptions

- (1–2) None.
- (3) `bad_expected_access<io::error>` when [parse](parse.md) finds no cookie: a first pair without a `=` or with a
  name that is not a token (`net::errc::invalid_cookie`).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::cookie theme("theme", "dark");
    theme.secure = true;
    println("{}", theme.to_string());

    net::http::cookie lang("lang=pl; Path=/docs; SameSite=strict");
    println("{} {} {} {}", lang.name, lang.value, lang.path, lang.same_site);
}
```

Output:

```text
theme=dark; Secure
lang pl /docs Strict
```

## See also

- [parse](parse.md): a `Set-Cookie` value from outside, its error returned
- [to_string](to_string.md): the cookie written
- [sgcl::net::http::cookie](README.md)
