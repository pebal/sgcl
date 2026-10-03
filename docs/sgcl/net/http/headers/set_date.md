[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](README.md)

# sgcl::net::http::headers::set_date

```cpp
headers& set_date(const string& name, const time::datetime& t) noexcept;
```

Sets the field `name` to the instant `t` written as IMF-fixdate, `Sun, 06 Nov 1994 08:49:37 GMT`, always in GMT
whatever the zone of `t`, Go's `http.TimeFormat`: [set](set.md) of the text, so that the value takes the place of the
first field of the name. The format is the `time` module's `time::http`; the fraction of a second is dropped.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, in any case |
| `t` | the instant, in any zone |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::datetime noon = time::datetime::parse("2024-05-01T12:00:00+02:00", time::rfc3339).value();
    net::http::headers h;
    h.set_date("Last-Modified", noon);
    println("{}", h.get("Last-Modified"));
    println("{}", h.date("last-modified") == noon);
}
```

Output:

```text
Wed, 01 May 2024 10:00:00 GMT
true
```

## See also

- [date](date.md): a date read
- [sgcl::net::http::headers](README.md)
