package newrelic

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"
	"time"

	"newrelic/collector"
	"newrelic/utilization"
)

// AgentRunID is a string as of agent listener protocol version 14.
type AgentRunID string

func (id AgentRunID) String() string {
	return string(id)
}

type AppState int

const (
	AppStateUnknown AppState = iota
	AppStateConnected
	AppStateInvalidLicense
	AppStateDisconnected
)

// An AppKey uniquely identifies an application.
type AppKey struct {
	License           collector.LicenseKey
	Appname           string
	RedirectCollector string
	HighSecurity      bool
	AgentLanguage     string
}

// AppInfo encapsulates information provided by an agent about an
// application. The information is used to construct part of the connect
// message sent to the collector, and the fields should not be modified.
type AppInfo struct {
	License           collector.LicenseKey
	Appname           string
	AgentLanguage     string
	AgentVersion      string
	HostDisplayName   string
	Settings          JSONString
	Environment       JSONString
	HighSecurity      bool
	Labels            JSONString
	RedirectCollector string
}

func (info *AppInfo) String() string {
	return info.Appname
}

// ConnectReply contains all of the fields from the app connect command reply
// that are used in the daemon.  The reply contains many more fields, but most
// of them are used in the agent.
type ConnectReply struct {
	ID          *AgentRunID            `json:"agent_run_id"`
	MetricRules MetricRules            `json:"metric_name_rules"`
	DataMethods *collector.DataMethods `json:"data_methods"`
}

// An App represents the state of an application.
type App struct {
	state              AppState
	collector          string
	lastConnectAttempt time.Time
	info               *AppInfo
	connectReply       *ConnectReply
	RawConnectReply    []byte
	HarvestTrigger     HarvestTriggerFunc
	LastActivity       time.Time
	Rules              MetricRules
}

func (app *App) String() string {
	return app.info.String()
}

func (info *AppInfo) Key() AppKey {
	return AppKey{
		License:           info.License,
		Appname:           info.Appname,
		RedirectCollector: info.RedirectCollector,
		HighSecurity:      info.HighSecurity,
		AgentLanguage:     info.AgentLanguage,
	}
}

func (app *App) Key() AppKey {
	return app.info.Key()
}

func NewApp(info *AppInfo) *App {
	now := time.Now()

	return &App{
		state:              AppStateUnknown,
		collector:          "",
		lastConnectAttempt: time.Time{},
		info:               info,
		HarvestTrigger:     nil,
		LastActivity:       now,
	}
}

func (info *AppInfo) ConnectJSONInternal(pid int, util *utilization.Data) ([]byte, error) {
	// Per spec, the hostname we send up in ConnectJSON MUST be the same as the
	// hostname we send up in Utilization.

	var hostname string
	if util != nil {
		hostname = util.Hostname
	}

	var data = struct {
		Pid             int               `json:"pid"`
		Language        string            `json:"language"`
		Version         string            `json:"agent_version"`
		Host            string            `json:"host"`
		HostDisplayName string            `json:"display_host,omitempty"`
		Settings        JSONString        `json:"settings"`
		AppName         []string          `json:"app_name"`
		HighSecurity    bool              `json:"high_security"`
		Labels          JSONString        `json:"labels"`
		Environment     JSONString        `json:"environment"`
		Identifier      string            `json:"identifier"`
		Util            *utilization.Data `json:"utilization,omitempty"`
	}{
		Pid:             pid,
		Language:        info.AgentLanguage,
		Version:         info.AgentVersion,
		Host:            hostname,
		HostDisplayName: stringLengthByteLimit(info.HostDisplayName, HostLengthByteLimit),
		Settings:        info.Settings,
		AppName:         strings.Split(info.Appname, ";"),
		HighSecurity:    info.HighSecurity,
		Environment:     info.Environment,
		// This identifier field is provided to avoid:
		// https://newrelic.atlassian.net/browse/DSCORE-778
		//
		// This identifier is used by the collector to look up the real agent. If an
		// identifier isn't provided, the collector will create its own based on the
		// first appname, which prevents a single daemon from connecting "a;b" and
		// "a;c" at the same time.
		//
		// Providing the identifier below works around this issue and allows users
		// more flexibility in using application rollups.
		Identifier: info.Appname,
		Util:       util,
	}

	if len(info.Labels) > 0 {
		data.Labels = info.Labels
	} else {
		data.Labels = JSONString("[]")
	}

	buf := &bytes.Buffer{}
	buf.Grow(2048)
	buf.WriteByte('[')

	enc := json.NewEncoder(buf)
	if err := enc.Encode(&data); err != nil {
		return nil, err
	}

	// json.Encoder always writes a trailing newline, replace it with the
	// closing bracket for the connect array.
	buf.Bytes()[buf.Len()-1] = ']'

	return buf.Bytes(), nil
}

// ConnectJSON creates the JSON of a connect request to be sent to the edge tier.
//
// Utilization is always expected to be present.
func (info *AppInfo) ConnectJSON(util *utilization.Data) ([]byte, error) {
	return info.ConnectJSONInternal(os.Getpid(), util)
}

func (app *App) NeedsConnectAttempt(now time.Time, backoff time.Duration) bool {
	if app.state != AppStateUnknown {
		return false
	}
	if now.Sub(app.lastConnectAttempt) >= backoff {
		return true
	}
	return false
}

func parseConnectReply(rawConnectReply []byte) (*ConnectReply, error) {
	var c ConnectReply

	err := json.Unmarshal(rawConnectReply, &c)
	if nil != err {
		return nil, err
	}
	if nil == c.ID {
		return nil, errors.New("missing agent run id")
	}
	return &c, nil
}

// Inactive determines whether the elapsed time since app last had activity
// exceeds a threshold.
func (app *App) Inactive(threshold time.Duration) bool {
	if threshold < 0 {
		panic(fmt.Errorf("invalid inactivity threshold: %v", threshold))
	}
	return time.Since(app.LastActivity) > threshold
}
