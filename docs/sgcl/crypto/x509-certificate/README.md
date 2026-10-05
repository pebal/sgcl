[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::certificate

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class certificate;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::certificate` is an X.509 certificate (RFC 5280) read from DER or PEM: its names, its validity,
its key and its extensions, each read once into a field of its own, and the verification of the chain from it to a
trusted root, with the host name or the IP address it must be for. Go's `x509.Certificate`.

Go's certificate is a struct of exported fields that any code may change after the parse; here the parse is done
once, the fields are read through `const` methods, and a copy of a certificate is a pointer to the same parse. What a
certificate may be used for is not a field to be read and trusted but the question of [verify](verify.md).

## Rules

- **A value that costs a pointer.** The parse is one managed object, shared by every copy and never changed: a
  certificate is copied, compared and read from many threads at once without a lock.
- **Made by [parse](parse.md) or [from_pem](from_pem.md)**, which return an
  [expected](../../core/expected/README.md): a certificate that cannot be read is `errc::malformed` with the offset of the
  byte, never an exception. There is no default constructor.
- **Everything is readable.** A key of an algorithm the module has no type for is `key_kind::none`
  ([public_key](../x509-public_key/README.md)), and the rest of the certificate is read as Go reads it; a certificate signed by
  such a key does not verify.
- **Never an exception from data.** A certificate, however broken, is `errc::malformed` from the readers or a
  verification error from the checks: a certificate cannot stop the program. The checks that take the program's own
  data throw for its mistakes: [verify_ip](verify_ip.md) an address of another length than 4 or
  16 bytes.
- **The views are of the certificate's bytes**: the `raw_*` slices keep them alive; the fields returned by reference
  are valid while a copy of the certificate lives.

## Member functions

#### Reading

| Function | Description |
|---|---|
| [parse](parse.md) | a certificate from its DER (static) |
| [from_pem](from_pem.md) | the first certificate of a PEM text (static) |

#### Encoding

| Function | Description |
|---|---|
| [raw](raw.md) | the whole DER |
| [raw_tbs](raw_tbs.md) | the TBSCertificate: what the signature covers |
| [raw_issuer](raw_issuer.md) | the issuer's name as encoded |
| [raw_subject](raw_subject.md) | the subject's name as encoded |
| [raw_subject_public_key_info](raw_subject_public_key_info.md) | the SubjectPublicKeyInfo |

#### Fields

| Function | Description |
|---|---|
| [version](version.md) | 1, 2 or 3 |
| [serial_number](serial_number.md) | the serial number, as encoded |
| [issuer](issuer.md) | the name of the issuer |
| [subject](subject.md) | the name of the subject |
| [not_before](not_before.md) | the start of the validity |
| [not_after](not_after.md) | the end of the validity |
| [public_key](public_key.md) | the subject's key |
| [signature_algorithm](signature_algorithm.md) | the algorithm of the signature |
| [signature_algorithm_oid](signature_algorithm_oid.md) | the algorithm's OID |
| [signature](signature.md) | the issuer's signature |
| [extensions](extensions.md) | every extension, in order |

#### Constraints and usages

| Function | Description |
|---|---|
| [has_basic_constraints](has_basic_constraints.md) | checks whether there is a basicConstraints |
| [is_ca](is_ca.md) | its cA |
| [max_path_length](max_path_length.md) | its pathLenConstraint |
| [has_key_usage](has_key_usage.md) | checks whether there is a keyUsage |
| [key_usage](key_usage.md) | the bits of the keyUsage |
| [allows](allows.md) | checks the bits of the keyUsage |
| [ext_key_usages](ext_key_usages.md) | the extended key usages known by name |
| [unknown_ext_key_usages](unknown_ext_key_usages.md) | the OIDs of the other extended key usages |

#### Subject alternative names

| Function | Description |
|---|---|
| [dns_names](dns_names.md) | the DNS names |
| [email_addresses](email_addresses.md) | the email addresses |
| [ip_addresses](ip_addresses.md) | the IP addresses |
| [uris](uris.md) | the URIs |

#### Key identifiers

| Function | Description |
|---|---|
| [authority_key_id](authority_key_id.md) | the issuer's key identifier |
| [subject_key_id](subject_key_id.md) | the subject's key identifier |

#### Name constraints

| Function | Description |
|---|---|
| [has_name_constraints](has_name_constraints.md) | checks whether there is a nameConstraints |
| [permitted_dns_domains, excluded_dns_domains](permitted_dns_domains.md) | the DNS subtrees |
| [permitted_ip_ranges, excluded_ip_ranges](permitted_ip_ranges.md) | the IP ranges |
| [permitted_email_addresses, excluded_email_addresses](permitted_email_addresses.md) | the mailboxes and mail domains |
| [permitted_uri_domains, excluded_uri_domains](permitted_uri_domains.md) | the domains of URIs |

#### Policies and critical extensions

| Function | Description |
|---|---|
| [policies](policies.md) | the OIDs of certificatePolicies |
| [must_staple](must_staple.md) | checks whether the TLS feature extension asks for an OCSP staple (Must-Staple) |
| [unhandled_critical_extensions](unhandled_critical_extensions.md) | the OIDs of the critical extensions not handled |

#### Revocation

| Function | Description |
|---|---|
| [ocsp_servers](ocsp_servers.md) | the URIs of its OCSP responders (authorityInfoAccess) |
| [issuing_certificate_urls](issuing_certificate_urls.md) | the URIs of its issuer's certificate (authorityInfoAccess) |
| [crl_distribution_points](crl_distribution_points.md) | the URIs of its CRL distribution points |

#### Verification

| Function | Description |
|---|---|
| [check_signature_from](check_signature_from.md) | checks that a parent signed the certificate |
| [verify_hostname](verify_hostname.md) | checks that the certificate is for a DNS name |
| [verify_ip](verify_ip.md) | checks that the certificate is for an IP address |
| [verify](verify.md) | the chain from the certificate to a trusted root |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the bytes |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

// a leaf for www.example.com, issued by the CA of ca_pem
const char* leaf_pem = R"(-----BEGIN CERTIFICATE-----
MIICkzCCAkWgAwIBAgICIAIwBQYDK2VwMIGoMQswCQYDVQQGEwJQTDEUMBIGA1UE
CAwLTWF6b3dpZWNraWUxETAPBgNVBAcMCFdhcnN6YXdhMREwDwYDVQQJDAhQcm9z
dGEgMTEPMA0GA1UEEQwGMDAtMDAxMRYwFAYDVQQKDA1FeGFtcGxlLCBJbmMuMQ0w
CwYDVQQLDAREb2NzMQswCQYDVQQFEwI0MjEYMBYGA1UEAwwPRXhhbXBsZSBEb2Nz
IENBMB4XDTI2MDEwMTAwMDAwMFoXDTM2MDEwMTAwMDAwMFowMjEWMBQGA1UECgwN
RXhhbXBsZSwgSW5jLjEYMBYGA1UEAwwPd3d3LmV4YW1wbGUuY29tMCowBQYDK2Vw
AyEACOK0QZ9l3+KUq5dE14RWTOPbltjhtnPvgEeTSJBRpamjggEGMIIBAjAMBgNV
HRMBAf8EAjAAMA4GA1UdDwEB/wQEAwIHgDApBgNVHSUEIjAgBggrBgEFBQcDAQYI
KwYBBQUHAwIGCisGAQQBgjcKAwwwYgYDVR0RBFswWYIPd3d3LmV4YW1wbGUuY29t
ghEqLmFwaS5leGFtcGxlLmNvbYERYWRtaW5AZXhhbXBsZS5jb22HBAoAAAeGGmh0
dHBzOi8vYXBwLmV4YW1wbGUuY29tL2lkMBMGA1UdIAQMMAowCAYGZ4EMAQICMB0G
A1UdDgQWBBTJgnNQwGozJMEAZ1Bzimov7/OiJDAfBgNVHSMEGDAWgBQfD1WKbZt/
hpSVb+K97uksyngRUjAFBgMrZXADQQCVajTwxc4gBqFIrntyNpf9l2W3o0pD0t+Q
a36iW1CIW/Uv9wasFFePQvIeCtkmV8PT/EgKlhaV/1uNfjgzKg4N
-----END CERTIFICATE-----
)";

int main() {
    crypto::x509::certificate leaf = crypto::x509::certificate::from_pem(leaf_pem);

    println("{}", leaf.subject().to_string());
    println("issued by {}, valid until {}", leaf.issuer().common_name(), leaf.not_after());
    for (auto& name : leaf.dns_names()) {
        println("  DNS {}", name);
    }
    bool ed25519 = leaf.public_key().kind() == crypto::x509::key_kind::ed25519;
    println("  {}", ed25519 ? "an Ed25519 key" : "another key");
}
```

Output:

```text
CN=www.example.com,O=Example\, Inc.
issued by Example Docs CA, valid until 2036-01-01T00:00:00Z
  DNS www.example.com
  DNS *.api.example.com
  an Ed25519 key
```

## See also

- [certificate_pool](../x509-certificate_pool/README.md): the roots and the intermediates
- [verify_options](../x509-verify_options.md): what a verification checks against
- [encoding::pem](../../encoding/pem/README.md): PEM itself
- [sgcl::crypto::x509](../x509.md)
