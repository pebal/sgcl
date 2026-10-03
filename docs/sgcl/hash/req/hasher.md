[sgcl](../../README.md) › [hash](../README.md) › req

# sgcl::hash::req::hasher

```cpp
#include "sgcl/hash/mixin/hasher.h"   // or "sgcl/hash.h"

namespace sgcl::hash::req {
    template<class H>
    concept hasher;  // H derives from mixin::hasher<H>
}
```

A hasher of the library: a type that said so by deriving from [mixin::hasher\<H\>](../mixin/hasher.md) with itself
as `H`. It is what a function over any hasher asks for, `hash::req::hasher auto& h`, and such a function then has
the whole shape: `update`, `value`, `digest`, `reset`, `of`, `copy_from`, `digest_size` and `block_size`. A `const`
or a reference on `H` is dropped first, so `req::hasher<const crc32&>` holds as well.

The requirement is nominal, as the [requirements of the containers](../../core/req.md) are: a type is a hasher
because it said so, not because it happens to have an `update`, and the error for anything else is one line at the
call. It is Go's `hash.Hash`, asked for statically.

## Satisfied by

- every hasher of this module: [crc32](../crc32.md), [crc32c](../crc32c.md), [crc64](../crc64.md),
  [crc64_iso](../crc64_iso.md), [adler32](../adler32.md), [fnv32](../fnv32.md), [fnv32a](../fnv32a.md),
  [fnv64](../fnv64.md), [fnv64a](../fnv64a.md), [fnv128](../fnv128.md), [fnv128a](../fnv128a.md),
  [xxh3_64](../xxh3_64.md), [xxh3_128](../xxh3_128.md), [maphash](../maphash.md), [siphash](../siphash.md);
- the digests of crypto: [sha1](../../crypto/sha1.md), [sha224 and sha256](../../crypto/sha256.md), [sha384, sha512
  and sha512_256](../../crypto/sha512.md), [sha3_224, sha3_256, sha3_384 and sha3_512](../../crypto/sha3_256.md), and
  [hmac\<H\>](../../crypto/hmac.md);
- a class of the program that derives from `mixin::hasher` with itself as the argument.

Not by a class derived from one of these: `struct mine : hash::crc32 {}` derives from `mixin::hasher<crc32>`, not
from `mixin::hasher<mine>`, and a function asking for `req::hasher` takes the `crc32` it is made of,
`static_cast<hash::crc32&>(d)`. Not by crypto's `shake128` and `shake256`, and not by a type with an `update` of
its own that does not derive from the mixin.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

// Any hasher: the digest of a text, in hexadecimal
string hex_digest(hash::req::hasher auto h, const string& text) {
    h.update(text);
    return encoding::hex::encode(h.digest());
}

struct mine : hash::crc32 {};

int main() {
    println("{}", hex_digest(hash::crc32(), "abc"));
    println("{}", hex_digest(crypto::sha256(), "abc"));
    println("{} {} {}", hash::req::hasher<hash::siphash>, hash::req::hasher<crypto::sha3_256>,
            hash::req::hasher<const hash::crc32&>);
    println("{} {}", hash::req::hasher<mine>, hash::req::hasher<crypto::shake128>);
}
```

Output:

```text
352441c2
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
true true true
false false
```

## See also

- [mixin::hasher](../mixin/hasher.md): the shape a hasher declares itself with
- [req](../../core/req.md): the requirements of core, nominal in the same way
- [sgcl::hash](../README.md)
