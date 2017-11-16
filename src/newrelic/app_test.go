package newrelic

import (
	"testing"
	"time"

	"newrelic/utilization"
)

func TestConnectJSONInternal(t *testing.T) {
	ramInitializer := new(uint64)
	*ramInitializer = 1000
	processors := 22
	util := &utilization.Data{
		MetadataVersion:   1,
		LogicalProcessors: &processors,
		RamMiB:            ramInitializer,
		Hostname:          "some_host",
	}
	info := &AppInfo{
		License:           "the_license",
		Appname:           "one;two",
		AgentLanguage:     "php",
		AgentVersion:      "0.1",
		HostDisplayName:   "my_awesome_host",
		Settings:          JSONString(`{"a": 1}`),
		Environment:       JSONString(`[["b", 2]]`),
		HighSecurity:      false,
		Labels:            JSONString(`[{"label_type":"c","label_value":"d"}]`),
		RedirectCollector: "staging-collector.newrelic.com",
	}

	pid := 123
	expected := `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"display_host":"my_awesome_host",` +
		`"settings":{"a":1},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"}` +
		`}` +
		`]`

	b, err := info.ConnectJSONInternal(pid, util)
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}

	// an empty string for the HostDisplayName should not produce JSON
	info.HostDisplayName = ""
	expected = `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"settings":{"a":1},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"}` +
		`}` +
		`]`

	b, err = info.ConnectJSONInternal(pid, util)
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Fatal(string(b))
	}
}

func TestNeedsConnectAttempt(t *testing.T) {
	var app App

	now := time.Date(2015, time.January, 10, 23, 0, 0, 0, time.UTC)

	app.state = AppStateUnknown
	app.lastConnectAttempt = now.Add(-AppConnectAttemptBackoff)
	if !app.NeedsConnectAttempt(now, AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateUnknown
	app.lastConnectAttempt = now
	if app.NeedsConnectAttempt(now, AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateConnected
	app.lastConnectAttempt = now.Add(-AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateInvalidLicense
	app.lastConnectAttempt = now.Add(-AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateDisconnected
	app.lastConnectAttempt = now.Add(-AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}
}
