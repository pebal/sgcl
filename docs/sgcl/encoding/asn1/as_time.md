[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_time

```cpp
optional<time::datetime> as_time() const noexcept;
```

The instant of a UTCTime or a GeneralizedTime, or of an implicitly tagged primitive read as one (as a
GeneralizedTime when it reads as one, else as a UTCTime): a [datetime](../../time/datetime/README.md) in UTC, or in
the fixed zone of the offset a time read as BER gives. A UTCTime's years are 1950 to 2049 (RFC 5280); a fraction of
a second is kept to the nanosecond. A GeneralizedTime past 2262 (RFC 5280's `99991231235959Z`, "no expiry") is the
last instant a `datetime` holds, and one before 1677 the first; [as_string](as_string.md) has the text. `nullopt`
for another type and for a time that is not one.

## Parameters

None.

## Return value

The instant, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    vector<byte> der = encoding::hex::decode("170d3233313131343232313332305a").value();
    auto t = encoding::asn1::parse(der)->as_time();
    println(t->to_string());
    vector<byte> ber = encoding::hex::decode("17113233313131343232313332302b30313030").value();
    println(encoding::asn1::parse(ber, encoding::asn1::ber)->as_time()->to_string());
    vector<byte> never = encoding::hex::decode("180f39393939313233313233353935395a").value();
    println(encoding::asn1::parse(never)->as_time()->year());
}
```

Output:

```text
2023-11-14T22:13:20Z
2023-11-14T22:13:20+01:00
2262
```

## See also

- [utc_time](utc_time.md), [generalized_time](generalized_time.md): one made
- [datetime](../../time/datetime/README.md)
- [sgcl::encoding::asn1](README.md)
