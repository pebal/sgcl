[sgcl](../../README.md) › [crypto](../README.md) › [nonce_counter](../nonce_counter.md)

# sgcl::crypto::nonce_counter::nonce_counter

```cpp
nonce_counter() noexcept = default;                               // (1)
explicit nonce_counter(const array<byte, 12>& start) noexcept;    // (2)
nonce_counter(nonce_counter&& other) noexcept;                    // (3)
nonce_counter(const nonce_counter&) = delete;                     // (4)
```

1. A counter from zero: its first nonce is twelve zero bytes.
2. A counter from `start` on: its first nonce is `start`. Where a program resumes the counter it kept, the last
   nonce it used plus one, or starts the nonces of a second sender of the same key in a range of their own.
3. Takes the place of `other` over; `other` is spent, and its `next()` throws `out_of_range`.
4. A counter is not copied: the copy would hand out the same nonces twice.

## Parameters

| Parameter | Description |
|---|---|
| `start` | the first nonce, a 96-bit number big-endian |
| `other` | the counter taken over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::nonce_counter fresh;
    println("{}", encoding::hex::encode(fresh.next()));

    // Resumed after a restart: the last nonce used was ...00ff
    array<byte, 12> start = {};
    start[10] = byte(0x01);
    crypto::nonce_counter resumed(start);
    println("{}", encoding::hex::encode(resumed.next()));

    crypto::nonce_counter taken = std::move(resumed);
    println("{}", encoding::hex::encode(taken.next()));
    try {
        resumed.next();
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
000000000000000000000000
000000000000000000000100
000000000000000000000101
sgcl::crypto::nonce_counter: every nonce has been used (or the counter was moved from)
```

## See also

- [next](next.md): the next nonce
- [operator=](operator_assign.md): takes another counter over
- [sgcl::crypto::nonce_counter](../nonce_counter.md)
