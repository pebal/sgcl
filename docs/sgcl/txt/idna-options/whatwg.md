[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna/README.md) › [options](README.md)

# sgcl::txt::idna::options::whatwg

```cpp
static constexpr options whatwg() noexcept;
```

Returns the profile the [WHATWG URL Standard](https://url.spec.whatwg.org/) asks for, what a browser's URL parser
wants: the deviations kept, the hyphens and the lengths not checked (`check_hyphens` and `verify_dns_length` off),
the bidirectional and joiner rules still checked. A looser profile on purpose, so that names already in the wild keep
working; [url](../../net/url/README.md) reads its hosts with it.

## Parameters

None.

## Return value

The defaults with `check_hyphens` and `verify_dns_length` set to `false`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr auto o = txt::idna::options::whatwg();
    println("{} {} {}", o.check_hyphens, o.verify_dns_length, o.check_bidi);
    for (auto name : {"x..ß", "ab--cd.com", "example.com."}) {
        println("{}", txt::idna::to_ascii(name, o).value());
    }
}
```

Output:

```text
false false true
x..xn--zca
ab--cd.com
example.com.
```

## See also

- [standard](standard.md): the strict profile
- [sgcl::txt::idna::options](README.md)
