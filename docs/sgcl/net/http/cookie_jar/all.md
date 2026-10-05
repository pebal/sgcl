[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::all

```cpp
vector<cookie> all() const noexcept;
```

Every cookie of the jar that has not expired, by domain and then in the order they were created, each as
[cookies](cookies.md) gives it: the domain with a dot in front for a domain cookie, the host alone for a host-only
one, `expires` for a persistent cookie. Their time of last use stays as it was.

## Parameters

None.

## Return value

The cookies.

## Complexity

Linear in the cookies of the jar, and their sort.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::cookie_jar jar;
    jar.set_cookies(net::url("https://www.example.com/"),
                    {net::http::cookie("a=1"), net::http::cookie("b=2; Domain=example.com; Max-Age=60")});
    jar.set_cookies(net::url("http://127.0.0.1:8080/x/y"), {net::http::cookie("c=3")});
    for (auto& c : jar.all()) {
        println("{} {} {} {}", c.name, c.domain, c.path, c.expires.has_value());
    }
}
```

Output:

```text
c 127.0.0.1 /x false
b .example.com / true
a www.example.com / false
```

## See also

- [size](size.md): how many
- [to_json](to_json.md): every cookie as text
- [sgcl::net::http::cookie_jar](README.md)
