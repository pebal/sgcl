[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::revocation_list

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class revocation_list;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::revocation_list` is a certificate revocation list (RFC 5280 §5): a CA's signed list of the
certificates it revoked, read from DER or PEM, with the status of a certificate by it. Go's `x509.RevocationList` of
`ParseRevocationList`, whose entries are structs of their own; here the entries stay in the list's bytes, indexed, so
that a list of a hundred thousand is read with one allocation for the index, and [operator\[\]](operator_at.md) makes
the value of one when it is asked for. [status_of](status_of.md) does what RFC 5280 §6.3 asks of a list before it
says anything: its signature, its issuer, its times, its scope, a delta over it.

## Rules

- **A value that costs a pointer.** The parse is one managed object, never changed: copies share it, and it is read
  from many threads at once without a lock.
- **Never an exception from data.** A list that cannot be read is `errc::malformed` from [parse](parse.md); one that
  cannot say, `errc::verification` with a [reason](../x509-reason.md) from [status_of](status_of.md). Only an index
  past the end, the program's, throws.
- **Delta CRLs** are read and applied over their complete list ([status_of](status_of.md) (2)); alone they say
  nothing. **Indirect CRLs** (entries of other issuers) are read and not used for a status.

## Member functions

| Function | Description |
|---|---|
| [parse](parse.md) | a list from its DER (static) |
| [from_pem](from_pem.md) | the first list of a PEM text (static) |

#### Encoding

| Function | Description |
|---|---|
| [raw](raw.md) | the whole DER |
| [raw_tbs](raw_tbs.md) | the TBSCertList: what the signature covers |
| [raw_issuer](raw_issuer.md) | the issuer's name as encoded |

#### Fields

| Function | Description |
|---|---|
| [version](version.md) | 1 or 2 |
| [issuer](issuer.md) | the name of the issuer |
| [this_update](this_update.md) | when it was issued |
| [next_update](next_update.md) | when the next one is due |
| [signature_algorithm](signature_algorithm.md) | the algorithm of the signature |
| [signature_algorithm_oid](signature_algorithm_oid.md) | the algorithm's OID |
| [signature](signature.md) | the issuer's signature |
| [extensions](extensions.md) | every extension, in order |
| [number](number.md) | its cRLNumber |
| [is_delta](is_delta.md) | checks whether it is a delta CRL |
| [base_number](base_number.md) | the number of the complete list a delta follows |
| [authority_key_id](authority_key_id.md) | the key of the issuer that signed it |

#### Scope

| Function | Description |
|---|---|
| [distribution_point](distribution_point.md) | the distribution point it is the list of |
| [only_user_certificates](only_user_certificates.md) | checks whether it covers end-entity certificates alone |
| [only_ca_certificates](only_ca_certificates.md) | checks whether it covers CA certificates alone |
| [indirect](indirect.md) | checks whether it lists certificates of other issuers |

#### Entries

| Function | Description |
|---|---|
| [size](size.md) | the number of certificates listed |
| [empty](empty.md) | checks whether it lists none |
| [operator\[\]](operator_at.md) | the entry at an index |
| [lookup](lookup.md) | the entry of a serial number or a certificate |

#### Verification

| Function | Description |
|---|---|
| [check_signature_from](check_signature_from.md) | checks whether a CA signed the list |
| [status_of](status_of.md) | the status of a certificate by the list, and a delta of it |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();

    for (const char* name : {"good.pem", "revoked.pem"}) {
        auto cert = crypto::x509::certificate::from_pem(io::read_text(dir + name)).value();
        auto status = crl.status_of(cert, issuer);
        println("{} {}",
            name, status == crypto::x509::revocation_status::revoked ? "revoked" : "good");
    }
}
```

Output:

```text
good.pem good
revoked.pem revoked
```

## See also

- [revoked_certificate](../x509-revoked_certificate.md): an entry
- [certificate::crl_distribution_points](../x509-certificate/crl_distribution_points.md): where a certificate's list is
- [ocsp_response](../x509-ocsp_response/README.md): the status of one certificate instead
- [net::tls::config](../../net/tls/config.md): `crls`, `fetch_crls`
- [sgcl::crypto::x509](../x509.md)
