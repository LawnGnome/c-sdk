package collector

import (
	"crypto/sha256"
	"encoding/base64"
	"net/url"
)

const (
	CommandMetrics      string = "metric_data"
	CommandErrors              = "error_data"
	CommandTraces              = "transaction_sample_data"
	CommandSlowSQLs            = "sql_trace_data"
	CommandCustomEvents        = "custom_event_data"
	CommandErrorEvents         = "error_event_data"
	CommandTxnEvents           = "analytic_event_data"
	CommandConnect             = "connect"
	CommandRedirect            = "get_redirect_host"
)

const (
	procotolVersion = "14"
)

// LicenseKey represents a license key for an account.
type LicenseKey string

func (cmd *Cmd) String() string {
	// TODO: Perhaps add license here.
	if cmd.RunID != "" {
		return cmd.Name + " " + cmd.RunID
	}
	return cmd.Name
}

func (cmd *Cmd) url(obfuscate bool) string {
	var u url.URL

	u.Host = cmd.Collector
	u.Path = "agent_listener/invoke_raw_method"

	if cmd.UseSSL {
		u.Scheme = "https"
	} else {
		u.Scheme = "http"
	}

	query := url.Values{}
	query.Set("marshal_format", "json")
	query.Set("protocol_version", procotolVersion)
	query.Set("method", cmd.Name)

	if obfuscate {
		query.Set("license_key", cmd.License.String())
	} else {
		query.Set("license_key", string(cmd.License))
	}

	if cmd.RunID != "" {
		query.Set("run_id", cmd.RunID)
	}

	u.RawQuery = query.Encode()
	return u.String()
}

// String obfuscates the license key by removing all but a safe prefix
// and suffix.
func (key LicenseKey) String() string {
	if n := len(key); n > 4 {
		return string(key[0:2] + ".." + key[n-2:])
	}
	return string(key)
}

func (key LicenseKey) Sha256() string {
	sum := sha256.Sum256([]byte(key))
	return base64.StdEncoding.EncodeToString(sum[:])
}
