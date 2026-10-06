// The other side of tests/net/http/oauth2.cpp: an OAuth 2.0 authorization
// server and OpenID provider written with Go's standard library alone, on a
// port of the loopback ("port N" on the first line); /quit ends it.
//
// Clients: "web" (secret "web-secret", confidential), "spa" (public, PKCE
// required), "short" (secret "short-secret": access tokens of 12 s, which a
// client's 10 s margin takes as expired after 2 s). The
// user is "ann". Endpoints: /authorize (approves at once: a 302 to the
// redirect with a code and the state), /token (authorization_code with
// PKCE S256, client_credentials, refresh_token rotating the refresh token,
// device_code), /device_authorization (a device code polled: two
// authorization_pending, one slow_down, then the token; "deny" in the
// scope makes it access_denied), /revoke, /introspect, /userinfo, /jwks
// (an ES256 key, kid "k1"), /.well-known/openid-configuration, /api (a
// resource: 200 for a live Bearer token, 401 error="invalid_token" else).
// Client authentication by Basic (client_secret_basic) or by the form
// (client_secret_post); a wrong secret is 401 invalid_client.
package main

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"
)

type grant struct {
	client, challenge, redirect, scope, nonce string
}

type tokenInfo struct {
	client, scope string
	exp           time.Time
	refresh       bool
}

var (
	mu      sync.Mutex
	codes   = map[string]grant{}
	tokens  = map[string]tokenInfo{}
	devices = map[string]*device{}
	key     *ecdsa.PrivateKey
	issuer  string
	secrets = map[string]string{"web": "web-secret", "short": "short-secret", "spa": ""}
)

type device struct {
	client, scope string
	polls         int
	last          time.Time
}

func random() string {
	b := make([]byte, 16)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(v)
}

func oauthError(w http.ResponseWriter, code int, err, desc string) {
	if code == 401 {
		w.Header().Set("WWW-Authenticate", `Basic realm="oauth"`)
	}
	writeJSON(w, code, map[string]string{"error": err, "error_description": desc})
}

// the client of a request: Basic, or client_id/client_secret of the form
func client(r *http.Request) (string, bool) {
	id, secret, basic := r.BasicAuth()
	if basic {
		id, _ = url.QueryUnescape(id)
		secret, _ = url.QueryUnescape(secret)
	} else {
		id, secret = r.PostForm.Get("client_id"), r.PostForm.Get("client_secret")
	}
	want, ok := secrets[id]
	if !ok {
		return "", false
	}
	if want != "" && want != secret {
		return "", false
	}
	return id, true
}

func b64(b []byte) string { return base64.RawURLEncoding.EncodeToString(b) }

func idToken(client, nonce string) string {
	head := b64([]byte(`{"alg":"ES256","kid":"k1","typ":"JWT"}`))
	claims, _ := json.Marshal(map[string]any{"iss": issuer, "sub": "ann", "aud": client, "exp": time.Now().Add(time.Hour).Unix(),
		"iat": time.Now().Unix(), "nonce": nonce, "email": "ann@example.org"})
	signing := head + "." + b64(claims)
	h := sha256.Sum256([]byte(signing))
	r, s, _ := ecdsa.Sign(rand.Reader, key, h[:])
	sig := make([]byte, 64)
	r.FillBytes(sig[:32])
	s.FillBytes(sig[32:])
	return signing + "." + b64(sig)
}

func issue(w http.ResponseWriter, client, scope, nonce string, refresh bool) {
	life := time.Hour
	if client == "short" {
		life = 12 * time.Second
	}
	access := random()
	out := map[string]any{"access_token": access, "token_type": "Bearer", "expires_in": int(life.Seconds()), "scope": scope}
	mu.Lock()
	tokens[access] = tokenInfo{client: client, scope: scope, exp: time.Now().Add(life)}
	if refresh {
		rt := random()
		tokens[rt] = tokenInfo{client: client, scope: scope, exp: time.Now().Add(24 * time.Hour), refresh: true}
		out["refresh_token"] = rt
	}
	mu.Unlock()
	if strings.Contains(" "+scope+" ", " openid ") {
		out["id_token"] = idToken(client, nonce)
	}
	writeJSON(w, 200, out)
}

func main() {
	key, _ = ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	issuer = "http://" + l.Addr().String()
	mux := http.NewServeMux()
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) { os.Exit(0) })
	mux.HandleFunc("/.well-known/openid-configuration", func(w http.ResponseWriter, r *http.Request) {
		writeJSON(w, 200, map[string]any{"issuer": issuer, "authorization_endpoint": issuer + "/authorize", "token_endpoint": issuer + "/token",
			"device_authorization_endpoint": issuer + "/device_authorization", "revocation_endpoint": issuer + "/revoke",
			"introspection_endpoint": issuer + "/introspect", "userinfo_endpoint": issuer + "/userinfo", "jwks_uri": issuer + "/jwks",
			"response_types_supported": []string{"code"}, "subject_types_supported": []string{"public"},
			"id_token_signing_alg_values_supported": []string{"ES256"}, "code_challenge_methods_supported": []string{"S256"}})
	})
	mux.HandleFunc("/jwks", func(w http.ResponseWriter, r *http.Request) {
		x, y := make([]byte, 32), make([]byte, 32)
		key.PublicKey.X.FillBytes(x)
		key.PublicKey.Y.FillBytes(y)
		writeJSON(w, 200, map[string]any{"keys": []any{map[string]string{"kty": "EC", "crv": "P-256", "kid": "k1", "use": "sig", "alg": "ES256", "x": b64(x), "y": b64(y)}}})
	})
	mux.HandleFunc("/authorize", func(w http.ResponseWriter, r *http.Request) {
		q := r.URL.Query()
		redirect := q.Get("redirect_uri")
		if _, ok := secrets[q.Get("client_id")]; !ok || q.Get("response_type") != "code" {
			http.Error(w, "unknown client", 400)
			return
		}
		if q.Get("client_id") == "spa" && q.Get("code_challenge") == "" {
			http.Redirect(w, r, redirect+"?error=invalid_request&error_description=PKCE+required&state="+url.QueryEscape(q.Get("state")), 302)
			return
		}
		if q.Get("code_challenge") != "" && q.Get("code_challenge_method") != "S256" {
			http.Redirect(w, r, redirect+"?error=invalid_request&state="+url.QueryEscape(q.Get("state")), 302)
			return
		}
		code := random()
		mu.Lock()
		codes[code] = grant{client: q.Get("client_id"), challenge: q.Get("code_challenge"), redirect: redirect, scope: q.Get("scope"), nonce: q.Get("nonce")}
		mu.Unlock()
		http.Redirect(w, r, redirect+"?code="+code+"&state="+url.QueryEscape(q.Get("state")), 302)
	})
	mux.HandleFunc("/token", func(w http.ResponseWriter, r *http.Request) {
		r.ParseForm()
		id, ok := client(r)
		if !ok {
			oauthError(w, 401, "invalid_client", "client authentication failed")
			return
		}
		switch r.PostForm.Get("grant_type") {
		case "authorization_code":
			mu.Lock()
			g, found := codes[r.PostForm.Get("code")]
			delete(codes, r.PostForm.Get("code"))
			mu.Unlock()
			if !found || g.client != id || g.redirect != r.PostForm.Get("redirect_uri") {
				oauthError(w, 400, "invalid_grant", "the code is not valid")
				return
			}
			if g.challenge != "" {
				h := sha256.Sum256([]byte(r.PostForm.Get("code_verifier")))
				if b64(h[:]) != g.challenge {
					oauthError(w, 400, "invalid_grant", "PKCE verification failed")
					return
				}
			}
			issue(w, id, g.scope, g.nonce, true)
		case "client_credentials":
			if secrets[id] == "" {
				oauthError(w, 400, "unauthorized_client", "a public client has no credentials")
				return
			}
			issue(w, id, r.PostForm.Get("scope"), "", false)
		case "refresh_token":
			rt := r.PostForm.Get("refresh_token")
			mu.Lock()
			t, found := tokens[rt]
			delete(tokens, rt)
			mu.Unlock()
			if !found || !t.refresh || t.client != id {
				oauthError(w, 400, "invalid_grant", "the refresh token is not valid")
				return
			}
			issue(w, id, t.scope, "", true)
		case "urn:ietf:params:oauth:grant-type:device_code":
			mu.Lock()
			d, found := devices[r.PostForm.Get("device_code")]
			if !found || d.client != id {
				mu.Unlock()
				oauthError(w, 400, "invalid_grant", "unknown device code")
				return
			}
			d.polls++
			polls := d.polls
			early := !d.last.IsZero() && time.Since(d.last) < 900*time.Millisecond
			d.last = time.Now()
			scope := d.scope
			mu.Unlock()
			switch {
			case strings.Contains(scope, "deny") && polls >= 2:
				oauthError(w, 400, "access_denied", "the user said no")
			case early:
				oauthError(w, 400, "slow_down", "polled too fast")
			case polls <= 2:
				oauthError(w, 400, "authorization_pending", "")
			default:
				issue(w, id, scope, "", true)
			}
		default:
			oauthError(w, 400, "unsupported_grant_type", r.PostForm.Get("grant_type"))
		}
	})
	mux.HandleFunc("/device_authorization", func(w http.ResponseWriter, r *http.Request) {
		r.ParseForm()
		id, ok := client(r)
		if !ok {
			oauthError(w, 401, "invalid_client", "")
			return
		}
		dc := random()
		mu.Lock()
		devices[dc] = &device{client: id, scope: r.PostForm.Get("scope")}
		mu.Unlock()
		writeJSON(w, 200, map[string]any{"device_code": dc, "user_code": "WDJB-MJHT", "verification_uri": issuer + "/device",
			"verification_uri_complete": issuer + "/device?user_code=WDJB-MJHT", "expires_in": 600, "interval": 1})
	})
	mux.HandleFunc("/revoke", func(w http.ResponseWriter, r *http.Request) {
		r.ParseForm()
		if _, ok := client(r); !ok {
			oauthError(w, 401, "invalid_client", "")
			return
		}
		mu.Lock()
		delete(tokens, r.PostForm.Get("token"))
		mu.Unlock()
		w.WriteHeader(200)
	})
	mux.HandleFunc("/introspect", func(w http.ResponseWriter, r *http.Request) {
		r.ParseForm()
		if _, ok := client(r); !ok {
			oauthError(w, 401, "invalid_client", "")
			return
		}
		mu.Lock()
		t, found := tokens[r.PostForm.Get("token")]
		mu.Unlock()
		if !found || time.Now().After(t.exp) {
			writeJSON(w, 200, map[string]any{"active": false})
			return
		}
		kind := "access_token"
		if t.refresh {
			kind = "refresh_token"
		}
		writeJSON(w, 200, map[string]any{"active": true, "scope": t.scope, "client_id": t.client, "username": "ann", "sub": "ann",
			"token_type": kind, "exp": t.exp.Unix(), "iat": time.Now().Unix(), "iss": issuer, "aud": []string{"api1", "api2"}})
	})
	bearer := func(r *http.Request) (tokenInfo, bool) {
		h := r.Header.Get("Authorization")
		if !strings.HasPrefix(h, "Bearer ") {
			return tokenInfo{}, false
		}
		mu.Lock()
		t, found := tokens[strings.TrimPrefix(h, "Bearer ")]
		mu.Unlock()
		return t, found && !t.refresh && time.Now().Before(t.exp)
	}
	mux.HandleFunc("/userinfo", func(w http.ResponseWriter, r *http.Request) {
		if _, ok := bearer(r); !ok {
			w.Header().Set("WWW-Authenticate", `Bearer error="invalid_token"`)
			w.WriteHeader(401)
			return
		}
		writeJSON(w, 200, map[string]any{"sub": "ann", "name": "Ann Example", "email": "ann@example.org"})
	})
	mux.HandleFunc("/api", func(w http.ResponseWriter, r *http.Request) {
		t, ok := bearer(r)
		if !ok {
			w.Header().Set("WWW-Authenticate", `Bearer realm="api", error="invalid_token", error_description="expired or unknown"`)
			w.WriteHeader(401)
			return
		}
		fmt.Fprintf(w, "data for %s", t.client)
	})
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	http.Serve(l, mux)
}
