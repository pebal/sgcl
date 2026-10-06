// The crypto module's cases that Go's standard library has, in Go
// (benchmarks/crypto/crypto.cpp has the SGCL and OpenSSL sides, the same
// cases, keys and lengths). Prints one line: ns per call and MB/s, after a
// quarter of a second thrown away and about two seconds timed.
//
//	crypto <case> [length=1024]
//
//	aes256cbc-encrypt      CBC encryption of `length` bytes (whole blocks), the
//	aes256cbc-decrypt      chain carried from call to call: cipher.NewCBCEncrypter
//	                       / NewCBCDecrypter's CryptBlocks, in place
//	p521-keygen            an ECDSA P-521 key generated
//	p521-sign              a signature of a SHA-512 digest (ecdsa.SignASN1)
//	p521-verify            its verification (ecdsa.VerifyASN1)
//	p521-ecdh              an ECDH shared secret with a fixed peer (crypto/ecdh)
//	totp                   a TOTP code of six digits, HMAC-SHA1 over the step
package main

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha1"
	"crypto/sha512"
	"encoding/binary"
	"fmt"
	"os"
	"strconv"
	"time"
)

var sink uint64

func runFor(f func() uint64, seconds float64, batch int) (uint64, float64) {
	var calls, acc uint64
	t0 := time.Now()
	wall := 0.0
	for wall < seconds {
		for i := 0; i < batch; i++ {
			acc += f()
		}
		calls += uint64(batch)
		wall = time.Since(t0).Seconds()
	}
	sink = acc
	return calls, wall
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: crypto <aes256cbc-encrypt|aes256cbc-decrypt|p521-keygen|p521-sign|p521-verify|p521-ecdh|totp> [length]")
		os.Exit(2)
	}
	what := os.Args[1]
	n := 1024
	if len(os.Args) > 2 {
		n, _ = strconv.Atoi(os.Args[2])
	}
	key := make([]byte, 32)
	for i := range key {
		key[i] = byte(i + 1)
	}
	data := make([]byte, n)
	for i := range data {
		data[i] = byte(i * 7)
	}
	digest := sha512.Sum512([]byte("the message"))
	batch := 64
	var f func() uint64
	switch what {
	case "aes256cbc-encrypt", "aes256cbc-decrypt":
		block, _ := aes.NewCipher(key)
		iv := make([]byte, 16)
		for i := range iv {
			iv[i] = 7
		}
		var mode cipher.BlockMode
		if what == "aes256cbc-encrypt" {
			mode = cipher.NewCBCEncrypter(block, iv)
		} else {
			mode = cipher.NewCBCDecrypter(block, iv)
		}
		whole := data[:n/16*16]
		f = func() uint64 {
			mode.CryptBlocks(whole, whole)
			return uint64(whole[0])
		}
	case "p521-keygen":
		f = func() uint64 {
			k, _ := ecdsa.GenerateKey(elliptic.P521(), rand.Reader)
			return uint64(k.D.Bits()[0])
		}
	case "p521-sign", "p521-verify":
		k, _ := ecdsa.GenerateKey(elliptic.P521(), rand.Reader)
		sig, _ := ecdsa.SignASN1(rand.Reader, k, digest[:])
		if what == "p521-sign" {
			f = func() uint64 {
				s, _ := ecdsa.SignASN1(rand.Reader, k, digest[:])
				return uint64(s[0])
			}
		} else {
			f = func() uint64 {
				if !ecdsa.VerifyASN1(&k.PublicKey, digest[:], sig) {
					panic("p521-verify")
				}
				return 1
			}
		}
	case "p521-ecdh":
		priv, _ := ecdh.P521().GenerateKey(rand.Reader)
		peer, _ := ecdh.P521().GenerateKey(rand.Reader)
		pub := peer.PublicKey()
		f = func() uint64 {
			s, _ := priv.ECDH(pub)
			return uint64(s[0])
		}
	case "totp":
		secret := []byte("12345678901234567890")
		var step uint64 = 59 / 30
		f = func() uint64 {
			var msg [8]byte
			binary.BigEndian.PutUint64(msg[:], step)
			step++
			m := hmac.New(sha1.New, secret)
			m.Write(msg[:])
			sum := m.Sum(nil)
			off := sum[len(sum)-1] & 15
			code := (binary.BigEndian.Uint32(sum[off:]) & 0x7fffffff) % 1000000
			return uint64(code)
		}
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	runFor(f, 0.25, batch)
	calls, wall := runFor(f, 2.0, batch)
	ns := wall * 1e9 / float64(calls)
	mbs := float64(n) * float64(calls) / wall / 1e6
	fmt.Printf("crypto %s go length=%d ns/op=%.1f MB/s=%.0f wall=%.2fs\n", what, n, ns, mbs, wall)
}
