[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::date

```cpp
optional<time::datetime> date() const;
```

The Date field as RFC 5322 §3.3 reads it, the obsolete forms of §4.3 too (a year of two digits, a zone's name,
comments): [time::email](../../time/layout/README.md).

## Parameters

None.

## Return value

The instant, in the field's offset; nothing when there is no Date or it is not a date.

## Complexity

Linear in the size of the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto m = encoding::email::parse("Date: 21 Nov 97 09:55:06 GMT\r\n\r\n").value();
    println("{}", m.date()->unix());
}
```

Output:

```text
880106106
```

## See also

- [set_date](set_date.md)
- [email](README.md)
