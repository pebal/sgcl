[sgcl](../../README.md) › [crypto](../README.md) › [aes_gcm](../aes_gcm.md)

# sgcl::crypto::aes_gcm::clone

```cpp
aes_gcm clone() const;
```

A second object with the same key: a copy of a secret, made on purpose and by name, where a copy constructor would
make one by accident. The two seal and open alike and are destroyed, and zeroed, each on its own.

## Parameters

None.

## Return value

An object with the key of this one.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Notes

One object seals and opens from any number of threads at once, so a clone is not needed to share a key between
threads; it is for a second owner, an object that outlives this one.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes_gcm gcm(vector<byte>(16));
    crypto::aes_gcm copy = gcm.clone();
    vector<byte> nonce(12);
    string text = "the same key";
    println("{}", gcm.seal(nonce, text) == copy.seal(nonce, text));

    crypto::aes_gcm taken = std::move(gcm);
    try {
        crypto::aes_gcm again = gcm.clone();
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::aes_gcm: used after being moved from
```

## See also

- [(constructor)](aes_gcm.md): sets up a key, or takes another object's over
- [sgcl::crypto::aes_gcm](../aes_gcm.md)
