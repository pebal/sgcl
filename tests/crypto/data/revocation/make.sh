#!/bin/sh
# The fixtures of the revocation tests (tests/crypto/x509_revocation.cpp,
# tests/net/tls_revocation.cpp), made once with OpenSSL 3 and committed:
#
#   root      a root CA (P-256), its CRL root.crl
#   int       an intermediate CA of root (P-256): AIA OCSP and CRL DP point
#             at the loopback (127.0.0.1:47811 the OCSP responder, :47812 the
#             CRLs), as the leaves' do; its complete CRL int.crl (number 2)
#             and a delta of it int_delta.crl (base 2)
#   responder a delegated OCSP responder of int (id-kp-OCSPSigning)
#   rogue     an OCSP signer of root, not of int: a responder int never named
#   good      leaf (localhost, 127.0.0.1) of int, not revoked
#   revoked   leaf of int, revoked (keyCompromise) in int.crl and in index.txt
#   held      leaf of int, on hold in int.crl, removed from the hold by the delta
#   staple    leaf of int with the TLS feature status_request (Must-Staple)
#   crlonly   leaf of int with a CRL DP and no AIA
#   unknown   leaf of int the responder's index does not hold
#   ocsp_*.der the responses of the responder for each (nextUpdate a century
#             ahead), ocsp_good_nonce.der with a nonce, ocsp_int_root.der root's
#             answer for int, ocsp_good_rogue.der signed by rogue
#
# Run from this directory: sh make.sh (overwrites everything here but itself)
set -e
O=/opt/homebrew/opt/openssl@3/bin/openssl
DAYS=36500
rm -rf work && mkdir work && touch work/root_index.txt work/int_index.txt
echo 01 > work/root_crlnumber; echo 01 > work/int_crlnumber
cat > work/ca.cnf <<CNF
[ ca ]
default_ca = int
[ root ]
dir = work
database = work/root_index.txt
new_certs_dir = work
certificate = root.pem
private_key = root.key
serial = work/root_serial
crlnumber = work/root_crlnumber
default_md = sha256
default_days = $DAYS
default_crl_days = $DAYS
policy = any
copy_extensions = none
unique_subject = no
[ int ]
dir = work
database = work/int_index.txt
new_certs_dir = work
certificate = int.pem
private_key = int.key
serial = work/int_serial
crlnumber = work/int_crlnumber
default_md = sha256
default_days = $DAYS
default_crl_days = $DAYS
policy = any
copy_extensions = none
unique_subject = no
[ any ]
commonName = supplied
[ v3_root ]
basicConstraints = critical,CA:true
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
[ v3_int ]
basicConstraints = critical,CA:true,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
authorityInfoAccess = OCSP;URI:http://127.0.0.1:47811
crlDistributionPoints = URI:http://127.0.0.1:47812/root.crl
[ v3_leaf ]
basicConstraints = critical,CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = serverAuth,clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
subjectAltName = DNS:localhost,IP:127.0.0.1
authorityInfoAccess = OCSP;URI:http://127.0.0.1:47811,caIssuers;URI:http://127.0.0.1:47812/int.cer
crlDistributionPoints = URI:http://127.0.0.1:47812/int.crl
[ v3_staple ]
basicConstraints = critical,CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = serverAuth,clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
subjectAltName = DNS:localhost,IP:127.0.0.1
authorityInfoAccess = OCSP;URI:http://127.0.0.1:47811
crlDistributionPoints = URI:http://127.0.0.1:47812/int.crl
tlsfeature = status_request
[ v3_crlonly ]
basicConstraints = critical,CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = serverAuth,clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
subjectAltName = DNS:localhost,IP:127.0.0.1
crlDistributionPoints = URI:http://127.0.0.1:47812/int.crl
[ v3_responder ]
basicConstraints = critical,CA:false
keyUsage = critical,digitalSignature
extendedKeyUsage = OCSPSigning
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
[ v3_delta ]
2.5.29.27 = critical,DER:02:01:02
authorityKeyIdentifier = keyid
[ v3_crl ]
authorityKeyIdentifier = keyid
CNF
key() { $O genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out $1.key; }
key root
$O req -new -x509 -key root.key -subj "/CN=SGCL Revocation Root" -days $DAYS -sha256 -config work/ca.cnf -extensions v3_root -out root.pem
echo 1000 > work/root_serial
echo 2000 > work/int_serial
sign_root() {   # name subject extensions
    key $1
    $O req -new -key $1.key -subj "$2" -out work/$1.csr
    $O ca -batch -config work/ca.cnf -name root -extensions $3 -in work/$1.csr -out work/$1.full -notext
    $O x509 -in work/$1.full -out $1.pem
}
sign_int() {
    key $1
    $O req -new -key $1.key -subj "$2" -out work/$1.csr
    $O ca -batch -config work/ca.cnf -name int -extensions $3 -in work/$1.csr -out work/$1.full -notext
    $O x509 -in work/$1.full -out $1.pem
}
sign_root int "/CN=SGCL Revocation Intermediate" v3_int
sign_root rogue "/CN=SGCL Rogue Responder" v3_responder
sign_int responder "/CN=SGCL OCSP Responder" v3_responder
sign_int good "/CN=good.localhost" v3_leaf
sign_int revoked "/CN=revoked.localhost" v3_leaf
sign_int held "/CN=held.localhost" v3_leaf
sign_int staple "/CN=staple.localhost" v3_staple
sign_int crlonly "/CN=crlonly.localhost" v3_crlonly
# unknown: signed by int but not in its index (a copy of the database before)
cp work/int_index.txt work/int_index_known.txt
sign_int unknown "/CN=unknown.localhost" v3_leaf
cp work/int_index_known.txt work/int_index.txt
$O ca -batch -config work/ca.cnf -name int -revoke revoked.pem -crl_reason keyCompromise
$O ca -batch -config work/ca.cnf -name int -revoke held.pem -crl_reason certificateHold
$O ca -batch -config work/ca.cnf -name int -gencrl -crlexts v3_crl -out work/int.crl.pem   # number 1
$O ca -batch -config work/ca.cnf -name int -gencrl -crlexts v3_crl -out work/int.crl.pem   # number 2
$O crl -in work/int.crl.pem -outform DER -out int.crl
# the delta: held removed from the hold (removeFromCRL)
cp work/int_index.txt work/int_index_full.txt
sed 's/certificateHold/removeFromCRL/' work/int_index_full.txt > work/int_index.txt
$O ca -batch -config work/ca.cnf -name int -gencrl -crlexts v3_delta -out work/int_delta.crl.pem
$O crl -in work/int_delta.crl.pem -outform DER -out int_delta.crl
cp work/int_index_full.txt work/int_index.txt
$O ca -batch -config work/ca.cnf -name root -gencrl -crlexts v3_crl -out work/root.crl.pem
$O crl -in work/root.crl.pem -outform DER -out root.crl
cp work/int_index.txt index.txt
cp work/root_index.txt root_index.txt
cat int.pem root.pem > chain.pem
resp() {   # out cert signer key [extra]
    $O ocsp -index index.txt -CA int.pem -rsigner $3.pem -rkey $3.key -issuer int.pem -cert $2.pem -ndays $DAYS -respout $1 $4 -reqout work/req.der > /dev/null
}
resp ocsp_good.der good responder -no_nonce
resp ocsp_revoked.der revoked responder -no_nonce
resp ocsp_unknown.der unknown responder -no_nonce
resp ocsp_good_issuer.der good int -no_nonce
resp ocsp_good_rogue.der good rogue -no_nonce
resp ocsp_staple.der staple responder -no_nonce
resp ocsp_good_nonce.der good responder ""
$O ocsp -index root_index.txt -CA root.pem -rsigner root.pem -rkey root.key -issuer root.pem -cert int.pem -ndays $DAYS -respout ocsp_int_root.der -no_nonce -reqout work/req.der > /dev/null
# requests made by OpenSSL: the oracle of the request builder
$O ocsp -issuer int.pem -cert good.pem -no_nonce -reqout ocsp_req_good_sha1.der
$O ocsp -issuer int.pem -sha256 -cert good.pem -no_nonce -reqout ocsp_req_good_sha256.der
$O ocsp -issuer int.pem -cert good.pem -reqout ocsp_req_good_nonce.der
rm -rf work
