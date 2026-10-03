[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::is_probable_prime

```cpp
bool is_probable_prime(int rounds = 20) const;
```

Whether the number is prime, as far as Baillie–PSW and `rounds` rounds of Miller–Rabin more can tell. Baillie–PSW
is trial division by the primes up to 211, the strong test to base 2 and the strong Lucas test with Selfridge's
parameters; the bases of the further rounds are drawn from a generator seeded by the number itself, so the same
number gets the same answer in every run and on every platform. A negative number, 0 and 1 are not prime.

Below 2^64 the answer is exact: no composite that small passes Baillie–PSW, and the further rounds are not run.
Above, no composite is known to pass it, and each further round lets through at most a quarter of the composites
that got so far. `rounds` is Go's argument to `ProbablyPrime`, with the same meaning: 0 is Baillie–PSW alone.

## Parameters

| Parameter | Description |
|---|---|
| `rounds` | the rounds of Miller–Rabin after Baillie–PSW, zero or more |

## Return value

`false` when the number is certainly composite (or below 2); `true` when it is prime, certainly below 2^64.

## Complexity

The trial division settles most composites. A number that passes it costs about `rounds + 3` modular
exponentiations at its length, about 3 below 2^64. A long number from outside is a long computation, as in any
library.

## Exceptions

`domain_error` when `rounds` is negative.

## Notes

Nothing here takes the same time whatever the value: the choosing of a prime for a key belongs to `crypto`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer m127 = math::big_integer(2).pow(127) - 1;
    println("{} {}", m127.is_probable_prime(), (m127 + 2).is_probable_prime());
    println("{} {} {}", math::big_integer(561).is_probable_prime(),
            math::big_integer(2).is_probable_prime(), math::big_integer(-7).is_probable_prime());
    math::big_integer n = math::big_integer(10).pow(30) + 1;
    while (!n.is_probable_prime()) {
        n += 2;
    }
    println("{}", n);  // the first prime past 10^30
}
```

Output:

```text
true false
false true false
1000000000000000000000000000057
```

## See also

- [mod_pow](mod_pow.md): the exponentiation the tests are made of
- [random::next_int](../random/next_int.md): a number below a bound, for a candidate
- [sgcl::math::big_integer](README.md)
