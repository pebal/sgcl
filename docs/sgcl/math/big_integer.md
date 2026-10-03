[sgcl](../README.md) › [math](README.md)

# sgcl::math::big_integer

```cpp
#include "sgcl/math/big_integer.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class big_integer;

    namespace literals {
        template<char... C>
        big_integer operator""_big() noexcept;
    }
}
```

`sgcl::math::big_integer` is a whole number of any size: what Go's `math/big.Int` is, with the manners of an
`int`. Operators and all: `a * 2 + 1` is written as it reads, `a < 10` and `a == 0` compare with the language's
own integers, `/` cuts towards zero and `%` takes the sign of the dividend as they do for `int`, and the methods
are `const` and hand back a new value: `a.to_string(16)`, `a.div_rem(d)`, `a.abs()`. Nothing overflows. The only
changes to a variable are the assignments, the compound assignments and the increments, which put a new value in
it.

Where Go's `big.Int` is a pointer and an operation writes its result into a receiver, `z.Add(x, y)`, a
`big_integer` is a value and `a + b` is a new one; where Go writes `big.NewInt(2)` for every constant in an
expression, the constant here is the `int` itself, and a constant longer than any integer of the language is the
literal [_big](big_integer/literals.md), whose digits the compiler checks, where Go has only `SetString` when the
program runs. A small value is inside the `big_integer` and allocates nothing, where Go allocates a slice of
words; and a copy is two words with the object of limbs shared, where Go's `new(big.Int).Set(x)` copies the words.

## Rules

- **Sixteen bytes, and a small value allocates nothing.** Every value `int64_t` holds lives inside the
  `big_integer`, always, so equality and the hash need no normalizing, and arithmetic on two of them is the
  processor's own with a check for overflow. A larger one lives in a managed object of limbs (words of 64 bits),
  made the way the characters of a [string](../core/string.md) are: of the smallest size class that holds them,
  each class a pool of its own, past about 62 KB a buffer of a range of pages. The limbs are numbers, never
  traced, never zeroed.
- **A value, shared by copying a word.** A copy of a `big_integer` is two words and a barrier; the object of limbs
  is shared, read by any number of threads without a lock, kept as a key of a [map](../core/map.md) or in an
  [immutable](../immutable/README.md) container, and never changed while two values have it. `a += b` means
  `a = a + b`: whoever else holds the old value still has it whole.
- **Written in place when nothing else has it.** The result of an operation that has not been copied since, `sum`
  in `sum += x`, `f` in `f *= k`, has its object to itself, and `+=`, `-=` and `*=` by a value within `int64_t`
  write into it while it has room, as Go's `z.Add(z, x)` does, without Go's receiver in the API. A copy (or a
  negation, which shares the limbs) marks the object shared, with one atomic write the first time, and from then
  on both values allocate their results as any value does; a move hands the object on and leaves zero behind.
  Nothing of this shows but in the time: a copy never changes with the value it was taken from.
- **Where it lives.** A `big_integer` holds a `tracked_ptr`, so it goes where one may: on a stack, in a managed
  object, in a container of the library ([The rules](../core/README.md#the-rules) of core). In a `std::vector` or
  a global, through [rooted](../core/rooted.md) or [root_ptr](../core/root_ptr.md).
- **An error of the program throws.** A division, a remainder or a `mod` by zero, a negative shift, a NaN or an
  infinity made into a number are `domain_error`; a base outside 2 to 36 is `invalid_argument`; `to_bytes(length)`
  too short and a shift past 2^46 limbs are `length_error`. Go panics in all of these, and `int` would be
  undefined.
- **An error of data does not.** [parse](big_integer/parse.md) of a text that is not a number is an
  [expected](../core/expected.md) whose [parse_error](parse_error.md) has `offset()`, the byte where the reading
  stopped, and `message()`, where Go's `SetString` answers `nil, false`.
- **Not constant-time.** The length of a value, the fast paths and the corrections of the division depend on the
  value. Public numbers only: a modulus, a serial number, an INTEGER of DER; secrets are `crypto`'s.
- **64-bit targets with `unsigned __int128`**: clang and gcc on arm64 and x86-64.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](big_integer/big_integer.md) | constructs a number: zero, from any whole number, from a `double`, from a literal text |
| `(destructor)` | drops the word; the object of limbs is left to the collector |
| [operator=](big_integer/operator_assign.md) | assigns another number: a copy shares its object, a move hands it on |

#### Arithmetic

| Function | Description |
|---|---|
| [operator+=, operator-=, operator\*=, operator/=, operator%=, operator-](big_integer/operator_arith.md) | the compound assignments and the negation |
| [operator&=, operator\|=, operator^=, operator\<\<=, operator\>\>=, operator~](big_integer/operator_arith.md) | the compound assignments of the bits and the complement |
| [operator++, operator--](big_integer/operator_inc.md) | adds or subtracts one, prefix and postfix |
| [sign](big_integer/sign.md) | -1, 0 or 1 |
| [abs](big_integer/abs.md) | the absolute value |
| [mod](big_integer/mod.md) | the remainder in `[0, \|m\|)`, whatever the signs |
| [div_rem](big_integer/div_rem.md) | the quotient and the remainder of one division |

#### Number theory

| Function | Description |
|---|---|
| [pow](big_integer/pow.md) | the number to a power |
| [sqrt](big_integer/sqrt.md) | the whole part of the square root |
| [gcd](big_integer/gcd.md) | the greatest common divisor |
| [lcm](big_integer/lcm.md) | the least common multiple |
| [mod_pow](big_integer/mod_pow.md) | a power modulo a number |
| [mod_inverse](big_integer/mod_inverse.md) | the inverse modulo a number, when there is one |
| [is_probable_prime](big_integer/is_probable_prime.md) | whether the number is prime, by Baillie–PSW and rounds of Miller–Rabin |
| [factorial](big_integer/factorial.md) | `n!` (static) |
| [binomial](big_integer/binomial.md) | the number of ways to choose `k` of `n` (static) |

#### Bits

| Function | Description |
|---|---|
| [bit_length](big_integer/bit_length.md) | the bits of the magnitude |
| [trailing_zeros](big_integer/trailing_zeros.md) | the zero bits below the lowest one |
| [bit](big_integer/bit.md) | one bit of the two's complement |

#### Conversions

| Function | Description |
|---|---|
| [parse](big_integer/parse.md) | reads a number from a text in a base, into an `expected` (static) |
| [from_bytes](big_integer/from_bytes.md) | the unsigned number of big-endian bytes (static) |
| [to_string](big_integer/to_string.md) | the digits in a base |
| [to_bytes](big_integer/to_bytes.md) | the magnitude as big-endian bytes |
| [to_int64](big_integer/to_int64.md) | the value as an `int64_t`, when it fits |
| [to_uint64](big_integer/to_uint64.md) | the value as a `uint64_t`, when it fits |
| [to_double](big_integer/to_double.md) | the nearest `double` |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-, operator\*, operator/, operator%](big_integer/operator_arith.md) | the arithmetic: `/` cut towards zero, `%` with the sign of the dividend |
| [operator&, operator\|, operator^, operator\<\<, operator\>\>](big_integer/operator_arith.md) | the bits in two's complement, the shifts |
| [operator==, operator\<=\>](big_integer/operator_cmp.md) | compare two numbers, or a number and any whole number of the language |
| [operator\<\<](big_integer/to_string.md) | writes the number to a stream as an `int` is written |
| [format_value](big_integer/format_value.md) | what `txt::format` writes for a number |
| [operator""_big](big_integer/literals.md) | a constant of any length, `0xffff'ffff'ffff'ffff'ffff_big` (`namespace literals`) |

## Specializations

```cpp
template<>
struct std::hash<sgcl::math::big_integer>;

template<>
struct sgcl::txt::formatter<sgcl::math::big_integer>;
```

`std::hash` hashes the value: equal numbers have equal hashes, a small value and a large one alike, so a `map` or
a `set` is keyed by numbers. The formatter tells [txt::format](../txt/format.md) which specifications a number
takes, so that a literal pattern is checked where it is compiled; the writing is
[format_value](big_integer/format_value.md)'s.

## Complexity

A value within `int64_t` costs what the processor's own arithmetic does and a branch; anything larger allocates
its result, one managed object per operation, from a pool of its size class, but for `+=`, `-=` and `*=` by a
small value on a value nobody else has, which write in place. The multiplication, the division and the
conversions take faster algorithms as the numbers grow ([operator_arith](big_integer/operator_arith.md),
[to_string](big_integer/to_string.md), [parse](big_integer/parse.md)), so a million digits either way take the
time of a few long multiplications. The number theory runs in the time of its arithmetic, and a long number from
outside is a long computation, as in any library. The times against Go's `math/big` are on
[benchmarks](benchmarks.md#big_integer).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer f = 1;
    for (int i : range(1, 101)) {
        f *= i;  // written in place: f is nobody else's
    }
    println("100! has {} digits, {} twos", f.to_string().size(), f.trailing_zeros());
    println("{}", f == math::big_integer::factorial(100));
    println("{}", f / math::big_integer::factorial(98) == 9900);
}
```

Output:

```text
100! has 158 digits, 97 twos
true
true
```

## See also

- [rational](rational.md): fractions of two `big_integer`s
- [random::next_int](random/next_int.md): a number drawn below a `big_integer`
- [parse_error](parse_error.md): why a text is not a number
- [string](../core/string.md): the model of an immutable object shared by a word that `big_integer` follows
- [The module](README.md)
