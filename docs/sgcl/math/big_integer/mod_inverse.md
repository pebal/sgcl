[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::mod_inverse

```cpp
optional<big_integer> mod_inverse(const big_integer& m) const;
```

The `x` in `[0, |m|)` with `a * x` congruent to 1 modulo `m`, as Go's `ModInverse`. A negative number is taken
modulo `m` first, and the sign of `m` does not matter. A number with a factor in common with `m` has no inverse and
gives nothing: that is an answer, not an error (Go's `ModInverse` returns `nil` there, Python's `pow(a, -1, m)`
raises). `m == 0` is the error. Modulo 1 the answer is 0.

The steps are those of [gcd](gcd.md), Lehmer's, with the coefficient carried along.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the modulus, not zero |

## Return value

The inverse in `[0, |m|)`, or `nullopt` when the number and `m` have a common factor.

## Complexity

Quadratic in the length of `m` (Lehmer's method, as Go's).

## Exceptions

`domain_error` when `m` is 0.

## Example

A toy RSA: two primes, a key, a message there and back.

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;
using namespace math::literals;

int main() {
    auto p = 0xd5bbb96d30086ec484eba3d7f9caeb07_big;
    auto q = 0xc9a92c4c5a2b8f6e2b8e7f2a3d9c1e05_big;
    while (!p.is_probable_prime()) {
        p += 2;
    }
    while (!q.is_probable_prime()) {
        q += 2;
    }
    math::big_integer n = p * q;
    math::big_integer e = 65537;
    auto d = e.mod_inverse((p - 1).lcm(q - 1));
    if (!d) {
        return 1;
    }
    math::big_integer text("hello", 36);  // a literal: constructed
    math::big_integer sealed = text.mod_pow(e, n);
    math::big_integer opened = sealed.mod_pow(*d, n);
    println(opened.to_string(36));
}
```

Output:

```text
hello
```

## See also

- [mod_pow](mod_pow.md): a power modulo a number
- [gcd](gcd.md): the greatest common divisor
- [sgcl::math::big_integer](README.md)
