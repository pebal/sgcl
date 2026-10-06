// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of benchmarks/crypto/jose.cpp: JOSE written by hand over the
// standard library (Go has no JOSE package), the same work a case. One line
// with ns/op.
//
//	jose <case>
package main

import (
	"bytes"
	"crypto"
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/elliptic"
	"crypto/hmac"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/sha512"
	"crypto/subtle"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"math/big"
	"os"
	"strings"
	"time"
)

var b64 = base64.RawURLEncoding
var sink int

func measure(what string, f func() int) {
	run := func(seconds float64) (int, float64) {
		calls := 0
		t0 := time.Now()
		wall := 0.0
		for wall < seconds {
			for i := 0; i < 16; i++ {
				sink += f()
			}
			calls += 16
			wall = time.Since(t0).Seconds()
		}
		return calls, wall
	}
	run(0.25)
	calls, wall := run(2.0)
	fmt.Printf("jose %s ns/op=%.1f wall=%.2fs\n", what, wall*1e9/float64(calls), wall)
}

type signer struct {
	alg    string
	sign   func(input []byte) []byte
	verify func(input, sig []byte) bool
}

func keyOf(what string) signer {
	switch {
	case strings.Contains(what, "hs256"):
		k := make([]byte, 32)
		rand.Read(k)
		mac := func(input []byte) []byte {
			m := hmac.New(sha256.New, k)
			m.Write(input)
			return m.Sum(nil)
		}
		return signer{"HS256", mac, func(input, sig []byte) bool { return hmac.Equal(mac(input), sig) }}
	case strings.Contains(what, "es256"):
		k, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		return signer{"ES256", func(input []byte) []byte {
			d := sha256.Sum256(input)
			r, s, _ := ecdsa.Sign(rand.Reader, k, d[:])
			out := make([]byte, 64)
			r.FillBytes(out[:32])
			s.FillBytes(out[32:])
			return out
		}, func(input, sig []byte) bool {
			if len(sig) != 64 {
				return false
			}
			d := sha256.Sum256(input)
			return ecdsa.Verify(&k.PublicKey, d[:], new(big.Int).SetBytes(sig[:32]), new(big.Int).SetBytes(sig[32:]))
		}}
	case strings.Contains(what, "es512"):
		k, _ := ecdsa.GenerateKey(elliptic.P521(), rand.Reader)
		return signer{"ES512", func(input []byte) []byte {
			d := sha512.Sum512(input)
			r, s, _ := ecdsa.Sign(rand.Reader, k, d[:])
			out := make([]byte, 132)
			r.FillBytes(out[:66])
			s.FillBytes(out[66:])
			return out
		}, func(input, sig []byte) bool {
			if len(sig) != 132 {
				return false
			}
			d := sha512.Sum512(input)
			return ecdsa.Verify(&k.PublicKey, d[:], new(big.Int).SetBytes(sig[:66]), new(big.Int).SetBytes(sig[66:]))
		}}
	case strings.Contains(what, "rs256"):
		k, _ := rsa.GenerateKey(rand.Reader, 2048)
		return signer{"RS256", func(input []byte) []byte {
			d := sha256.Sum256(input)
			s, _ := rsa.SignPKCS1v15(rand.Reader, k, crypto.SHA256, d[:])
			return s
		}, func(input, sig []byte) bool {
			d := sha256.Sum256(input)
			return rsa.VerifyPKCS1v15(&k.PublicKey, crypto.SHA256, d[:], sig) == nil
		}}
	}
	pub, priv, _ := ed25519.GenerateKey(rand.Reader)
	return signer{"EdDSA", func(input []byte) []byte { return ed25519.Sign(priv, input) },
		func(input, sig []byte) bool { return ed25519.Verify(pub, input, sig) }}
}

func claims() map[string]any {
	return map[string]any{"iss": "https://id.example", "sub": "alice", "aud": "api", "iat": 1700000000, "exp": 2000000000}
}

func signJWT(s signer, c map[string]any) string {
	h, _ := json.Marshal(map[string]any{"alg": s.alg, "typ": "JWT"})
	p, _ := json.Marshal(c)
	input := b64.EncodeToString(h) + "." + b64.EncodeToString(p)
	return input + "." + b64.EncodeToString(s.sign([]byte(input)))
}

func verifyJWT(s signer, token string, now float64) (map[string]any, error) {
	parts := strings.Split(token, ".")
	if len(parts) != 3 {
		return nil, errors.New("parts")
	}
	hb, err := b64.DecodeString(parts[0])
	if err != nil {
		return nil, err
	}
	var h map[string]any
	if err := json.Unmarshal(hb, &h); err != nil {
		return nil, err
	}
	if h["alg"] != s.alg {
		return nil, errors.New("alg")
	}
	sig, err := b64.DecodeString(parts[2])
	if err != nil || !s.verify([]byte(parts[0]+"."+parts[1]), sig) {
		return nil, errors.New("signature")
	}
	pb, err := b64.DecodeString(parts[1])
	if err != nil {
		return nil, err
	}
	var c map[string]any
	if err := json.Unmarshal(pb, &c); err != nil {
		return nil, err
	}
	exp, ok := c["exp"].(float64)
	if !ok || now >= exp+60 {
		return nil, errors.New("exp")
	}
	if nbf, ok := c["nbf"].(float64); ok && now+60 < nbf {
		return nil, errors.New("nbf")
	}
	if iat, ok := c["iat"].(float64); ok && iat > now+60 {
		return nil, errors.New("iat")
	}
	if c["aud"] != "api" {
		return nil, errors.New("aud")
	}
	return c, nil
}

func concatKdf(z []byte, algID string, size int) []byte {
	h := sha256.New()
	var be [4]byte
	binary.BigEndian.PutUint32(be[:], 1)
	h.Write(be[:])
	h.Write(z)
	binary.BigEndian.PutUint32(be[:], uint32(len(algID)))
	h.Write(be[:])
	h.Write([]byte(algID))
	binary.BigEndian.PutUint32(be[:], 0)
	h.Write(be[:])
	h.Write(be[:])
	binary.BigEndian.PutUint32(be[:], uint32(size*8))
	h.Write(be[:])
	return h.Sum(nil)[:size]
}

func cbcTag(macKey, aad, iv, ct []byte) []byte {
	m := hmac.New(sha256.New, macKey)
	m.Write(aad)
	m.Write(iv)
	m.Write(ct)
	var al [8]byte
	binary.BigEndian.PutUint64(al[:], uint64(len(aad))*8)
	m.Write(al[:])
	return m.Sum(nil)[:16]
}

func main() {
	what := os.Args[1]
	now := 1800000000.0
	switch {
	case strings.HasPrefix(what, "jwt_") && strings.HasSuffix(what, "_sign"):
		s := keyOf(what)
		c := claims()
		measure(what, func() int { return len(signJWT(s, c)) })
	case strings.HasPrefix(what, "jwt_"):
		s := keyOf(what)
		token := signJWT(s, claims())
		measure(what, func() int {
			c, err := verifyJWT(s, token, now)
			if err != nil {
				panic(err)
			}
			return len(c["sub"].(string))
		})
	case what == "jwe_dir_a256gcm":
		key := make([]byte, 32)
		rand.Read(key)
		data := bytes.Repeat([]byte("x"), 1024)
		measure(what, func() int {
			h, _ := json.Marshal(map[string]any{"alg": "dir", "enc": "A256GCM"})
			protected := b64.EncodeToString(h)
			block, _ := aes.NewCipher(key)
			g, _ := cipher.NewGCM(block)
			iv := make([]byte, 12)
			rand.Read(iv)
			sealed := g.Seal(nil, iv, data, []byte(protected))
			n := len(data)
			token := strings.Join([]string{protected, "", b64.EncodeToString(iv), b64.EncodeToString(sealed[:n]), b64.EncodeToString(sealed[n:])}, ".")
			// and back
			parts := strings.Split(token, ".")
			hb, _ := b64.DecodeString(parts[0])
			var hh map[string]any
			json.Unmarshal(hb, &hh)
			iv2, _ := b64.DecodeString(parts[2])
			ct, _ := b64.DecodeString(parts[3])
			tag, _ := b64.DecodeString(parts[4])
			block2, _ := aes.NewCipher(key)
			g2, _ := cipher.NewGCM(block2)
			pt, err := g2.Open(nil, iv2, append(ct, tag...), []byte(parts[0]))
			if err != nil {
				panic(err)
			}
			return len(pt)
		})
	case what == "jwe_ecdh_es_a128cbc":
		priv, _ := ecdh.P256().GenerateKey(rand.Reader)
		pub := priv.PublicKey()
		data := bytes.Repeat([]byte("x"), 1024)
		measure(what, func() int {
			eph, _ := ecdh.P256().GenerateKey(rand.Reader)
			z, _ := eph.ECDH(pub)
			e := eph.PublicKey().Bytes()
			epk := map[string]any{"kty": "EC", "crv": "P-256", "x": b64.EncodeToString(e[1:33]), "y": b64.EncodeToString(e[33:])}
			h, _ := json.Marshal(map[string]any{"alg": "ECDH-ES", "enc": "A128CBC-HS256", "epk": epk})
			protected := b64.EncodeToString(h)
			cek := concatKdf(z, "A128CBC-HS256", 32)
			block, _ := aes.NewCipher(cek[16:])
			iv := make([]byte, 16)
			rand.Read(iv)
			pad := 16 - len(data)%16
			ct := append(append([]byte{}, data...), bytes.Repeat([]byte{byte(pad)}, pad)...)
			cipher.NewCBCEncrypter(block, iv).CryptBlocks(ct, ct)
			tag := cbcTag(cek[:16], []byte(protected), iv, ct)
			token := strings.Join([]string{protected, "", b64.EncodeToString(iv), b64.EncodeToString(ct), b64.EncodeToString(tag)}, ".")
			// and back
			parts := strings.Split(token, ".")
			hb, _ := b64.DecodeString(parts[0])
			var hh map[string]any
			json.Unmarshal(hb, &hh)
			ep := hh["epk"].(map[string]any)
			x, _ := b64.DecodeString(ep["x"].(string))
			y, _ := b64.DecodeString(ep["y"].(string))
			peer, err := ecdh.P256().NewPublicKey(append(append([]byte{4}, x...), y...))
			if err != nil {
				panic(err)
			}
			z2, _ := priv.ECDH(peer)
			cek2 := concatKdf(z2, "A128CBC-HS256", 32)
			iv2, _ := b64.DecodeString(parts[2])
			ct2, _ := b64.DecodeString(parts[3])
			tag2, _ := b64.DecodeString(parts[4])
			if subtle.ConstantTimeCompare(cbcTag(cek2[:16], []byte(parts[0]), iv2, ct2), tag2) != 1 {
				panic("tag")
			}
			block2, _ := aes.NewCipher(cek2[16:])
			out := make([]byte, len(ct2))
			cipher.NewCBCDecrypter(block2, iv2).CryptBlocks(out, ct2)
			return len(out) - int(out[len(out)-1])
		})
	case what == "jwk_parse_rsa_verify":
		k, _ := rsa.GenerateKey(rand.Reader, 2048)
		e := big.NewInt(int64(k.E)).Bytes()
		text, _ := json.Marshal(map[string]any{"kty": "RSA", "n": b64.EncodeToString(k.N.Bytes()), "e": b64.EncodeToString(e), "kid": "k1"})
		s := signer{"RS256", func(input []byte) []byte {
			d := sha256.Sum256(input)
			sig, _ := rsa.SignPKCS1v15(rand.Reader, k, crypto.SHA256, d[:])
			return sig
		}, nil}
		token := signJWT(s, claims())
		measure(what, func() int {
			var m map[string]any
			if err := json.Unmarshal(text, &m); err != nil {
				panic(err)
			}
			n, _ := b64.DecodeString(m["n"].(string))
			eb, _ := b64.DecodeString(m["e"].(string))
			pub := &rsa.PublicKey{N: new(big.Int).SetBytes(n), E: int(new(big.Int).SetBytes(eb).Int64())}
			v := signer{"RS256", nil, func(input, sig []byte) bool {
				d := sha256.Sum256(input)
				return rsa.VerifyPKCS1v15(pub, crypto.SHA256, d[:], sig) == nil
			}}
			c, err := verifyJWT(v, token, now)
			if err != nil {
				panic(err)
			}
			return len(c["sub"].(string)) + len(m["kid"].(string))
		})
	case what == "jwk_parse_ec":
		k, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		b, _ := k.PublicKey.ECDH()
		p := b.Bytes()
		text, _ := json.Marshal(map[string]any{"kty": "EC", "crv": "P-256", "x": b64.EncodeToString(p[1:33]), "y": b64.EncodeToString(p[33:]), "kid": "k1"})
		measure(what, func() int {
			var m map[string]any
			if err := json.Unmarshal(text, &m); err != nil {
				panic(err)
			}
			x, _ := b64.DecodeString(m["x"].(string))
			y, _ := b64.DecodeString(m["y"].(string))
			pub, err := ecdh.P256().NewPublicKey(append(append([]byte{4}, x...), y...))
			if err != nil {
				panic(err)
			}
			return len(pub.Bytes()) + len(m["kid"].(string))
		})
	default:
		fmt.Fprintln(os.Stderr, "jose: unknown case", what)
		os.Exit(2)
	}
}
