[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::uuid

```cpp
#include "sgcl/encoding/uuid.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class uuid;
}
```

`sgcl::encoding::uuid` is a universally unique identifier of [RFC 9562](https://www.rfc-editor.org/rfc/rfc9562):
sixteen bytes, written `f81d4fae-7dec-11d0-a765-00a0c91e6bf6`. It is a plain value of sixteen bytes with no pointer,
so a constant, a global and a key of a table, compared and hashed by its bytes. [v4](v4.md) makes a random one and
[v7](v7.md) one that begins with the time, so that keys made one after another sort in the order they were made
and land together in an index. Every version is read: [version](version.md), [variant](variant.md) and, for 1, 6
and 7, the [timestamp](timestamp.md). Go's standard library has none; its counterpart is `github.com/google/uuid`.

## Rules

- **A literal is checked at compile time**: `encoding::uuid id("f81d4fae-7dec-11d0-a765-00a0c91e6bf6");`, and
  one that is not a UUID is an error of the compiler. A text from outside is [parse](parse.md)d, in any of the four
  forms Go and Python read.
- **v4** is 122 random bits from the thread's ChaCha8 stream, keyed from the system and keyed again in the child
  of a `fork`, as RFC 9562 §6.9 asks of the randomness of an identifier (not a key: a secret is crypto's).
- **v7** is the Unix time in milliseconds, a counter of 12 bits and 62 random bits. Every v7 of a process is
  greater than the one before it, on any thread, even when the clock steps back: within one millisecond the counter
  counts, and when it is full it carries into the millisecond (§6.2, method 1).
- **v3 and v5 are read, not made**: their MD5 and SHA-1 are the crypto module's, which lies above this one. v1
  and v6 are read too; RFC 9562 itself recommends v7 over them. v8, a layout of one's own, is
  [the constructor](uuid.md) of sixteen bytes.
- The order is the order of the bytes (§6.11), which for v7 is the order of time.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `variant_kind` | the layout the variant field names: [uuid::variant_kind](../uuid-variant_kind.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](uuid.md) | the nil UUID, of a literal, of a text, of sixteen bytes |
| [parse](parse.md) | a UUID of a text (static) |
| [from_bytes](from_bytes.md) | a UUID of sixteen bytes (static) |

#### Making one

| Function | Description |
|---|---|
| [v4](v4.md) | a random UUID (static) |
| [v7](v7.md) | a UUID of the time, in order (static) |
| [nil](nil.md) | all zeros (static) |
| [max](max.md) | all ones (static) |

#### Observers

| Function | Description |
|---|---|
| [bytes](bytes.md) | the sixteen bytes |
| [version](version.md) | the version field |
| [variant](variant.md) | the variant field |
| [timestamp](timestamp.md) | the instant of a v1, v6 or v7 |
| [is_nil](is_nil.md) | whether it is all zeros |

#### Conversions

| Function | Description |
|---|---|
| [to_string](to_string.md) | the text, 8-4-4-4-12 |
| [hash](hash.md) | a hash of the bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\\<=\\>](operator_cmp.md) | equality and the order of the bytes |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::uuid id = encoding::uuid::v4();
    println("{} characters, version {}", id.to_string().size(), id.version());

    auto read = encoding::uuid::parse("urn:uuid:017f22e2-79b0-7cc3-98c4-dc0c0c07398f");
    if (!read) {
        println(read.error().message());
        return 1;
    }
    println("{} v{} made {}", *read, read->version(), read->timestamp()->to_string());
    println(encoding::uuid::v7() < encoding::uuid::v7());
}
```

Output:

```text
36 characters, version 4
017f22e2-79b0-7cc3-98c4-dc0c0c07398f v7 made 2022-02-22T19:22:22Z
true
```

## See also

- [hex](../hex/README.md): bytes as text
- [asn1::oid](../asn1-oid/README.md): an identifier of a registry
- [sgcl::encoding](../README.md)
