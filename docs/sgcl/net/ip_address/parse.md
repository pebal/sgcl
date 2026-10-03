[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::parse

```cpp
static expected<ip_address, io::error> parse(const string& text) noexcept;
```

Reads an IPv4 or an IPv6 address, as Go's `netip.ParseAddr` reads one: `"10.0.0.1"`, `"2001:db8::1"`, `"fe80::1%en0"`,
`"::ffff:1.2.3.4"`.

IPv4 is the dotted decimal form alone: four fields of 0 to 255, none with a leading zero. `"010.0.0.1"` is not an
address, since the C library reads a leading zero as octal and a program that checked the text would disagree with the
socket about where it connects (Go refuses it since 1.17); neither is `"127.1"` nor a hex field. The lenient forms a
browser takes belong to the URL parser ([url](../url.md)).

IPv6 is read by RFC 4291 §2.2: groups of one to four hex digits in either case, one `::` standing for one or more zero
groups (never for none: `1:2:3:4:5:6:7:8::` is refused), an embedded IPv4 address in the last 32 bits, a zone after
`%`. The zone is at most fifteen bytes, any of them but a NUL, a bracket included; a longer zone, or one with a NUL,
is refused, where Go keeps it. No brackets (those belong to an [endpoint](../endpoint.md)), no spaces around the
address.

Nothing is allocated for an address: only a text that is not one makes an error, which holds the text.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |

## Return value

The address, or an [io::error](../../io/error.md) of the code `net::errc::invalid_address` ([errc](../errc.md)), the
operation `parse IP address` and the text: its message reads `parse IP address 010.0.0.1: invalid address`.

## Complexity

Linear in the length of `text`.

## Exceptions

None.

## Notes

The text a program spells itself is constructed, `net::ip_address("10.0.0.1")` ([constructor](ip_address.md)), and a
wrong one throws; a text from outside the program is parsed. The corpus of `tools/ip_oracle.go` holds this reading to
Go's byte for byte.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"192.168.0.1", "2001:0DB8::0001", "fe80::1%en0", "::ffff:1.2.3.4",
                             "010.0.0.1", "127.1", "1:2:3:4:5:6:7:8::", "[::1]"}) {
        auto a = net::ip_address::parse(text);
        println("{} -> {}", text, a ? a->to_string() : a.error().message());
    }
}
```

Output:

```text
192.168.0.1 -> 192.168.0.1
2001:0DB8::0001 -> 2001:db8::1
fe80::1%en0 -> fe80::1%en0
::ffff:1.2.3.4 -> ::ffff:1.2.3.4
010.0.0.1 -> parse IP address 010.0.0.1: invalid address
127.1 -> parse IP address 127.1: invalid address
1:2:3:4:5:6:7:8:: -> parse IP address 1:2:3:4:5:6:7:8::: invalid address
[::1] -> parse IP address [::1]: invalid address
```

## See also

- [(constructor)](ip_address.md): the address a literal spells
- [to_string](to_string.md): the text back
- [endpoint::parse](../endpoint/parse.md), [ip_network::parse](../ip_network/parse.md): an address with a port, with a
  prefix length
- [sgcl::net::ip_address](../ip_address.md)
