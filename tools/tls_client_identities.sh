#!/bin/sh
# The client certificate of the fuzzing harnesses of mTLS (tests/net/fuzz/tls_*_fuzz.cpp,
# through tests/net/fuzz/tls_fuzz_settings.h), taken from tests/net/tls_testdata
# (tools/tls_client_testdata.sh) and written as tests/net/tls_client_identities.h:
# the CA's certificate, the ECDSA P-256 client leaf it signs and that leaf's key
# (PKCS #8), each DER in hex. From the root of the tree:
#
#   sh tools/tls_client_identities.sh > tests/net/tls_client_identities.h
set -e
openssl=${OPENSSL:-/opt/homebrew/opt/openssl@3/bin/openssl}
d=tests/net/tls_testdata
hex() {
    xxd -p | tr -d '\n'
}
cat <<'HEAD'
// Made by tools/tls_client_identities.sh (OpenSSL 3): do not edit.
#pragma once

namespace tls_client_identities {
HEAD
printf '    inline const char* ca = "%s";\n' "$("$openssl" x509 -in $d/ca.pem -outform DER | hex)"
printf '    inline const char* client_certificate = "%s";\n' "$("$openssl" x509 -in $d/client_ecdsa.pem -outform DER | hex)"
printf '    inline const char* client_key = "%s";\n' "$("$openssl" pkcs8 -topk8 -nocrypt -in $d/client_ecdsa.key -outform DER | hex)"
echo "}"
