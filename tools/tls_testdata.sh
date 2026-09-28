#!/bin/sh
# The certificates of the TLS interoperability tests (tests/net/tls_interop.cpp):
# a CA (ECDSA P-256) and three leaves it signs, for localhost and 127.0.0.1 —
# Ed25519, ECDSA P-256 and RSA 2048 (CertificateVerify's rsa_pss_rsae) — each
# key in PKCS #8. Test keys only: they guard nothing. From the root of the tree:
#
#     sh tools/tls_testdata.sh tests/net/tls_testdata
set -e
out=${1:-tests/net/tls_testdata}
openssl=${OPENSSL:-/opt/homebrew/opt/openssl@3/bin/openssl}
mkdir -p "$out"
cd "$out"
"$openssl" genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out ca.key
"$openssl" req -x509 -new -key ca.key -sha256 -days 36500 -subj "/CN=sgcl test CA" \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out ca.pem
printf "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n" > leaf.ext
for kind in ed25519 ecdsa rsa; do
    case $kind in
        ed25519) "$openssl" genpkey -algorithm ED25519 -out $kind.key ;;
        ecdsa) "$openssl" genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out $kind.key ;;
        rsa) "$openssl" genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out $kind.key ;;
    esac
    "$openssl" req -new -key $kind.key -subj "/CN=localhost" -out $kind.csr
    "$openssl" x509 -req -in $kind.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 36500 -sha256 -extfile leaf.ext -out $kind.pem
    rm $kind.csr
done
rm -f leaf.ext ca.srl
