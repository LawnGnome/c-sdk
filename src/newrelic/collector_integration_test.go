// +build integration

package newrelic

import (
	"encoding/json"
	"os"
	"testing"
	"time"

	"newrelic/collector"
	"newrelic/utilization"
)

func sampleConnectJSON(lic collector.LicenseKey) []byte {
	info := AppInfo{
		License:       lic,
		Appname:       "Unit Test Application",
		AgentLanguage: "php",
		AgentVersion:  "1.2.3",
		Settings:      nil,
		Environment:   nil,
		HighSecurity:  false,
		Labels:        nil,
	}
	js, err := info.ConnectJSON(utilization.Gather(utilization.Config{
		DetectAWS:    false,
		DetectAzure:  false,
		DetectGCP:    false,
		DetectPCF:    false,
		DetectDocker: false,
	}))
	if nil != err {
		return nil
	}
	return js
}

func sampleErrorData(id AgentRunID) ([]byte, error) {
	d := json.RawMessage(`[1418769.232,"WebTransaction/action/reaction","Unit Test Error Message","Unit Test Error Class",{}]`)

	h := NewErrorHeap(1)
	h.AddError(1, d)

	return h.Data(id, time.Now())
}

func testCommuncation(t *testing.T, license collector.LicenseKey, redirectCollector string, useSSL bool) {
	testClient, err := NewClient(&ClientConfig{})
	if nil != err {
		t.Fatal(err)
	}

	var invalidLicense collector.LicenseKey = "invalid_license_key"

	args := &ConnectArgs{
		RedirectCollector: redirectCollector,
		Payload:           sampleConnectJSON(license),
		License:           license,
		UseSSL:            useSSL,
		Client:            testClient,
	}

	// Invalid Connect
	args.License = invalidLicense
	connectAttempt := ConnectApplication(args)
	if !collector.IsLicenseException(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if "" != connectAttempt.Collector {
		t.Fatal(connectAttempt.Collector)
	}
	if nil != connectAttempt.Reply {
		t.Fatal(connectAttempt.Reply)
	}

	// Successful Connect
	args.License = license
	connectAttempt = ConnectApplication(args)
	if nil != connectAttempt.Err {
		t.Fatal(connectAttempt.Err)
	}
	if nil == connectAttempt.Reply {
		t.Fatal(connectAttempt.Reply)
	}
	if "" == connectAttempt.Collector {
		t.Fatal(connectAttempt.Collector)
	}

	reply, err := parseConnectReply(connectAttempt.RawReply)
	if nil != err {
		t.Fatal(err)
	}
	if nil == reply {
		t.Fatal(reply)
	}

	var out []byte
	call := collector.Cmd{
		UseSSL:    useSSL,
		Collector: connectAttempt.Collector,
		License:   license,
	}

	call.Name = collector.CommandErrors
	call.RunID = string(*reply.ID)
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		data, err := sampleErrorData(*reply.ID)
		if nil != err {
			t.Fatal(err)
		}
		return data, nil
	})

	// Invalid Error Command
	call.License = invalidLicense
	out, err = testClient.Execute(call)
	if nil != out {
		t.Fatal(out)
	}
	if collector.ErrUnauthorized != err {
		t.Fatal(err)
	}

	// Malformed Error Command
	call.License = license
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		return []byte("{"), nil
	})

	out, err = testClient.Execute(call)
	if nil != err {
		t.Fatal(err)
	}
	if string("null") != string(out) {
		t.Fatal(string(out))
	}

	// Valid Error Command
	call.License = license
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		data, err := sampleErrorData(*reply.ID)
		if nil != err {
			t.Fatal(err)
		}
		return data, nil
	})

	out, err = testClient.Execute(call)
	if nil != err {
		t.Fatal(err)
	}
	if string("null") != string(out) {
		t.Fatal(string(out))
	}
}

func TestRPM(t *testing.T) {
	var license1 collector.LicenseKey
	var redirectCollector string

	if "production" == os.Getenv("NEW_RELIC_ENV") {
		redirectCollector = "collector.newrelic.com"
		license1 = "951809161a1a07acc7da550284de0b6b09320be2" // Production account 820618 (rrh+php1)
		// license2 = "a4f1eebc409769318c590b8aa7d2ee4864e3489f" // Production account 820619 (rrh+php2)
	} else {
		redirectCollector = "staging-collector.newrelic.com"
		license1 = "9851c5a3b9bd2e4295c4a8712120548889d99296" // Staging account 17833 (Kean Test)
		// license2 := "4e8c0ad1c4e63058f78cea33765a9ef36004393b" // Staging account 204549 (willhf)
	}

	testCommuncation(t, license1, redirectCollector, true)
	testCommuncation(t, license1, redirectCollector, false)
}
