// Revocation in Go, the Go side of benchmarks/net/net.cpp's revocation
// cases (CASES=net in benchmarks/compare.sh): golang.org/x/crypto/ocsp and
// crypto/x509's revocation lists, the fixtures of
// tests/crypto/data/revocation (run from the root of the tree). Prints the
// line of the net cases, "net <case> ns/op=...".
//
//	ocsp_verify [n]     ocsp.ParseResponseForCert of the fixtures' good response (a delegated responder:
//	                    its certificate checked under the issuer, the response under it): per response
//	crl_make <dir>      CRLs of the intermediate of 1000 and of 100000 entries (serials 1..k times 7919, revoked
//	                    at one time, keyCompromise each), DER, <dir>/crl_1000.der and crl_100000.der, for both
//	                    sides' crl_1k and crl_100k
//	crl_1k [n]          x509.ParseRevocationList of $SGCL_CRL_DIR/crl_1000.der, CheckSignatureFrom the
//	                    intermediate, the entry of its last serial looked up: per list
//	crl_100k [n]        the same of crl_100000.der
//	tls_staple [n]      a TCP connection and a full TLS 1.3 handshake on the loopback (X25519, the fixtures'
//	                    good leaf and its intermediate), the server stapling the good response, the client
//	                    verifying it (VerifyConnection: ocsp.ParseResponseForCert), one byte read, both
//	                    closed: per connection
//	tls_nostaple [n]    the same without a staple and without its check
//
// A module of its own (golang.org/x/crypto, which the benchmarks' module
// does not require): compare.sh writes its go.mod when missing and builds it
// from the module cache.
package main

import (
	"crypto"
	"crypto/tls"
	"crypto/x509"
	"encoding/pem"
	"fmt"
	"io"
	"math/big"
	"net"
	"os"
	"strconv"
	"syscall"
	"time"

	"golang.org/x/crypto/ocsp"
)

const dir = "tests/crypto/data/revocation/"

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops float64) {
	fmt.Printf("net %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall*1e9/ops, ops/wall, wall, cpuSeconds())
}

func cert(name string) *x509.Certificate {
	b, err := os.ReadFile(dir + name + ".pem")
	if err != nil {
		panic(err)
	}
	blk, _ := pem.Decode(b)
	c, err := x509.ParseCertificate(blk.Bytes)
	if err != nil {
		panic(err)
	}
	return c
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: ocsp <ocsp_verify|crl_make|crl_1k|crl_100k|tls_staple|tls_nostaple> ...")
		os.Exit(2)
	}
	what := os.Args[1]
	arg := func(i int) int64 {
		if len(os.Args) > i {
			v, _ := strconv.ParseInt(os.Args[i], 10, 64)
			return v
		}
		return 0
	}
	switch what {
	case "ocsp_verify":
		n := arg(2)
		if n == 0 {
			n = 20000
		}
		der, err := os.ReadFile(dir + "ocsp_good.der")
		if err != nil {
			panic(err)
		}
		leaf, issuer := cert("good"), cert("int")
		check := func() bool {
			r, err := ocsp.ParseResponseForCert(der, leaf, issuer)
			return err == nil && r.Status == ocsp.Good
		}
		for i := 0; i < 200; i++ {
			check()
		}
		ok := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if check() {
				ok++
			}
		}
		report("ocsp_verify", time.Since(t0).Seconds(), float64(n))
		if int64(ok) != n {
			os.Exit(1)
		}
	case "crl_make":
		for _, k := range []int{1000, 100000} {
			makeCRL(k, fmt.Sprintf("%s/crl_%d.der", os.Args[2], k))
		}
	case "crl_1k", "crl_100k":
		file := os.Getenv("SGCL_CRL_DIR") + "/crl_1000.der"
		if what == "crl_100k" {
			file = os.Getenv("SGCL_CRL_DIR") + "/crl_100000.der"
		}
		der, err := os.ReadFile(file)
		if err != nil {
			panic(err)
		}
		n := arg(2)
		if n == 0 {
			n = 2000
			if what == "crl_100k" {
				n = 40
			}
		}
		issuer := cert("int")
		var last *big.Int
		check := func() bool {
			rl, err := x509.ParseRevocationList(der)
			if err != nil || rl.CheckSignatureFrom(issuer) != nil {
				return false
			}
			if last == nil {
				last = rl.RevokedCertificateEntries[len(rl.RevokedCertificateEntries)-1].SerialNumber
			}
			for i := range rl.RevokedCertificateEntries {
				if rl.RevokedCertificateEntries[i].SerialNumber.Cmp(last) == 0 {
					return true
				}
			}
			return false
		}
		check()
		ok := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if check() {
				ok++
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n))
		if int64(ok) != n {
			os.Exit(1)
		}
	case "tls_staple", "tls_nostaple":
		n := arg(2)
		if n == 0 {
			n = 2000
		}
		// the chain: the leaf and its intermediate
		chain := append(must(os.ReadFile(dir+"good.pem")), must(os.ReadFile(dir+"int.pem"))...)
		pair, err := tls.X509KeyPair(chain, must(os.ReadFile(dir+"good.key")))
		if err != nil {
			panic(err)
		}
		staple := what == "tls_staple"
		if staple {
			pair.OCSPStaple = must(os.ReadFile(dir + "ocsp_good.der"))
		}
		roots := x509.NewCertPool()
		roots.AddCert(cert("root"))
		curves := []tls.CurveID{tls.X25519}
		scfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS13, CurvePreferences: curves}
		ccfg := &tls.Config{RootCAs: roots, ServerName: "localhost", MinVersion: tls.VersionTLS13, CurvePreferences: curves}
		if staple {
			ccfg.VerifyConnection = func(cs tls.ConnectionState) error {
				chain := cs.VerifiedChains[0]
				r, err := ocsp.ParseResponseForCert(cs.OCSPResponse, chain[0], chain[1])
				if err != nil {
					return err
				}
				if r.Status != ocsp.Good {
					return fmt.Errorf("status %d", r.Status)
				}
				return nil
			}
		}
		l, err := tls.Listen("tcp", "127.0.0.1:0", scfg)
		if err != nil {
			panic(err)
		}
		served := make(chan int64)
		go func() {
			var ok int64
			for i := int64(0); i < n+1; i++ {
				c, err := l.Accept()
				if err != nil {
					break
				}
				go func(c net.Conn) {
					c.Write([]byte{'x'})
					c.Close()
				}(c)
				ok++
			}
			served <- ok
		}()
		dial := func(count int64) int64 {
			var ok int64
			buf := make([]byte, 1)
			for i := int64(0); i < count; i++ {
				c, err := tls.Dial("tcp", l.Addr().String(), ccfg)
				if err != nil {
					fmt.Fprintln(os.Stderr, err)
					break
				}
				if _, err := io.ReadFull(c, buf); err == nil {
					ok++
				}
				c.Close()
			}
			return ok
		}
		warm := dial(1)
		t0 := time.Now()
		done := dial(n)
		report(what, time.Since(t0).Seconds(), float64(n))
		got := <-served
		l.Close()
		if warm != 1 || done != n || got != n+1 {
			os.Exit(1)
		}
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
}

// A CRL of k entries of the intermediate, written to out
func makeCRL(k int, out string) {
	issuer := cert("int")
	kb, err := os.ReadFile(dir + "int.key")
	if err != nil {
		panic(err)
	}
	blk, _ := pem.Decode(kb)
	key, err := x509.ParsePKCS8PrivateKey(blk.Bytes)
	if err != nil {
		panic(err)
	}
	at := time.Unix(1790000000, 0).UTC()
	entries := make([]x509.RevocationListEntry, k)
	for i := range entries {
		entries[i] = x509.RevocationListEntry{SerialNumber: big.NewInt(int64(i+1) * 7919), RevocationTime: at, ReasonCode: 1}
	}
	der, err := x509.CreateRevocationList(zeroReader{}, &x509.RevocationList{
		RevokedCertificateEntries: entries, Number: big.NewInt(7), ThisUpdate: at, NextUpdate: at.Add(100 * 365 * 24 * time.Hour),
		SignatureAlgorithm: x509.ECDSAWithSHA256,
	}, issuer, key.(crypto.Signer))
	if err != nil {
		panic(err)
	}
	if err := os.WriteFile(out, der, 0o644); err != nil {
		panic(err)
	}
}

func must(b []byte, err error) []byte {
	if err != nil {
		panic(err)
	}
	return b
}

// ECDSA signs with randomness; a reader of zeros keeps crl_make's output
// one list for one k (crypto/ecdsa mixes it with the key and the digest)
type zeroReader struct{}

func (zeroReader) Read(p []byte) (int, error) {
	for i := range p {
		p[i] = 0
	}
	return len(p), nil
}
