package collector

import "testing"

func TestParseProxy(t *testing.T) {
	testCases := []struct {
		proxy, scheme, host string
	}{
		{"http://localhost", "http", "localhost"},
		{"http://localhost:8080", "http", "localhost:8080"},
		{"http://user@localhost:8080", "http", "localhost:8080"},
		{"http://user:pass@localhost:8080", "http", "localhost:8080"},
		{"https://localhost", "https", "localhost"},
		{"https://localhost:8080", "https", "localhost:8080"},
		{"https://user@localhost:8080", "https", "localhost:8080"},
		{"https://user:pass@localhost:8080", "https", "localhost:8080"},
		{"localhost", "http", "localhost"},
		{"localhost:8080", "http", "localhost:8080"},
		{"user@localhost:8080", "http", "localhost:8080"},
		{"user:pass@localhost:8080", "http", "localhost:8080"},
	}

	for _, tt := range testCases {
		url, err := parseProxy(tt.proxy)
		if err != nil {
			t.Errorf("parseProxy(%q) = %v", tt.proxy, err)
			continue
		}
		if url.Scheme != tt.scheme || url.Host != tt.host {
			t.Errorf("parseProxy(%q) = {Scheme: %q, Host: %q}\nwant {Scheme: %q, Host: %q}",
				tt.proxy, url.Scheme, url.Host, tt.scheme, tt.host)
		}
	}
}
