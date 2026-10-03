[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::host_address

```cpp
optional<ip_address> host_address() const noexcept;
```

The host as an [ip_address](../ip_address.md) when it is one, IPv4 or IPv6. The host of a scheme that is not special
is opaque, and never an address: `sc://1.2.3.4/` has the host `1.2.3.4`, a text.

## Parameters

None.

## Return value

The address, or `nullopt` for a name, for no host and for an opaque host.

## Complexity

Linear in the length of the part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"http://10.0.0.1/", "http://0x7f.1/", "http://[::1]:80/", "http://example.com/",
                             "sc://1.2.3.4/"}) {
        auto a = net::url(text).host_address();
        println("{} {}", text, a ? a->to_string() : string("none"));
    }
}
```

Output:

```text
http://10.0.0.1/ 10.0.0.1
http://0x7f.1/ 127.0.0.1
http://[::1]:80/ ::1
http://example.com/ none
sc://1.2.3.4/ none
```

## See also

- [hostname](hostname.md): the host as a text
- [sgcl::net::url](../url.md)
