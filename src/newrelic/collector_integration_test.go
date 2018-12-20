// +build integration

package newrelic

import (
	"encoding/json"
	"newrelic/collector"
	"newrelic/utilization"
	"testing"
	"time"
)

func sampleConnectPayload(lic collector.LicenseKey) *RawConnectPayload {
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

	return info.ConnectPayload(utilization.Gather(utilization.Config{
		DetectAWS:    false,
		DetectAzure:  false,
		DetectGCP:    false,
		DetectPCF:    false,
		DetectDocker: false,
	}))
}

func sampleErrorData(id AgentRunID) ([]byte, error) {
	d := json.RawMessage(`[1418769.232,"WebTransaction/action/reaction","Unit Test Error Message","Unit Test Error Class",{}]`)

	h := NewErrorHeap(1)
	h.AddError(1, d)

	return h.Data(id, time.Now())
}

func testCommuncation(t *testing.T, license collector.LicenseKey, redirectCollector string, securityToken string, supportedPolicies AgentPolicies) {
	testClient, err := NewClient(&ClientConfig{})
	if nil != err {
		t.Fatal("License: ", license, "Error: ", err)
	}

	var invalidLicense collector.LicenseKey = "invalid_license_key"
	var invalidToken string = "invalid_token"

	args := &ConnectArgs{
		RedirectCollector:            redirectCollector,
		PayloadRaw:                   sampleConnectPayload(license),
		License:                      license,
		Client:                       testClient,
		SecurityPolicyToken:          securityToken,
		AppSupportedSecurityPolicies: supportedPolicies,
	}

	// Invalid Connect
	args.License = invalidLicense
	connectAttempt := ConnectApplication(args)
	if !collector.IsLicenseException(connectAttempt.Err) {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
	}
	if "" != connectAttempt.Collector {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
	}
	if nil != connectAttempt.Reply {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
	}
	args.License = license

	// Test invalid conditions while attempting to
	// Connect. LASP and non-LASP accounts
	if "" != args.SecurityPolicyToken {
		// LASP accounts

		// Invalid Token
		args.SecurityPolicyToken = invalidToken
		connectAttempt := ConnectApplication(args)
		if !collector.IsDisconnect(connectAttempt.Err) {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
		}
		if "" != connectAttempt.Collector {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
		}
		if nil != connectAttempt.Reply {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
		}

		// Empty Token
		args.SecurityPolicyToken = ""
		connectAttempt = ConnectApplication(args)
		if !collector.IsDisconnect(connectAttempt.Err) {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
		}
		if "" != connectAttempt.Collector {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
		}
		if nil != connectAttempt.Reply {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
		}

	} else {
		// Non-LASP accounts

		// Non-empty Token
		args.SecurityPolicyToken = invalidToken
		connectAttempt := ConnectApplication(args)
		if !collector.IsDisconnect(connectAttempt.Err) {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
		}
		if "" != connectAttempt.Collector {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
		}
		if nil != connectAttempt.Reply {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
		}
	}
	args.SecurityPolicyToken = securityToken

	// Successful Connect
	connectAttempt = ConnectApplication(args)
	if nil != connectAttempt.Err {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
	}
	if nil == connectAttempt.Reply {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
	}
	if "" == connectAttempt.Collector {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
	}

	reply, err := parseConnectReply(connectAttempt.RawReply)
	if nil != err {
		t.Fatal("License: ", license, "Error: ", err)
	}
	if nil == reply {
		t.Fatal("License: ", license, "Error: ", reply)
	}

	var out []byte
	call := collector.Cmd{
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

/*
 * Initiate a connection attempt via ConnectApplication().
 * Expectations: Valid license key and valid token. Expect the Daemon to reject the connection attempt.
 * Checks are performed to confirm the rejection came from the Daemon and not the server.
 */
func testExpectDaemonDisconnect(t *testing.T, license collector.LicenseKey, redirectCollector string, securityToken string, supportedPolicies AgentPolicies) {
	testClient, err := NewClient(&ClientConfig{})
	if nil != err {
		t.Fatal(err)
	}

	args := &ConnectArgs{
		RedirectCollector:            redirectCollector,
		PayloadRaw:                   sampleConnectPayload(license),
		License:                      license,
		Client:                       testClient,
		SecurityPolicyToken:          securityToken,
		AppSupportedSecurityPolicies: supportedPolicies,
	}

	connectAttempt := ConnectApplication(args)
	// Confirm err is not a server error
	if collector.IsLicenseException(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if collector.IsDisconnect(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if collector.IsRestartException(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if collector.IsRuntime(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	// Confirm error is not null (something did go wrong)
	if nil == connectAttempt.Err {
		t.Fatal("no connection attempt error found when expected")
	}
	if "" != connectAttempt.Collector {
		t.Fatal(connectAttempt.Collector)
	}
	if nil != connectAttempt.Reply {
		t.Fatal(connectAttempt.Reply)
	}
}

func TestRPM(t *testing.T) {
	var license1 collector.LicenseKey
	var redirectCollector string
	var securityToken string
	var supportedPolicies AgentPolicies
	json.Unmarshal([]byte(`{"agent_policies": {"record_sql": {"enabled": true,"supported": true}}}`), &supportedPolicies)

	/* Test communication against a non-LASP enabled production account */

	redirectCollector = "collector.newrelic.com"
	license1 = "951809161a1a07acc7da550284de0b6b09320be2" // Production account 820618 (rrh+php1)
	// license2 = "a4f1eebc409769318c590b8aa7d2ee4864e3489f" // Production account 820619 (rrh+php2)

	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)
	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)

	/* Test communication against a non-LASP enabled staging account */

	redirectCollector = "staging-collector.newrelic.com"
	license1 = "9851c5a3b9bd2e4295c4a8712120548889d99296" // Staging account 17833 (Kean Test)
	// license2 := "4e8c0ad1c4e63058f78cea33765a9ef36004393b" // Staging account 204549 (willhf)

	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)
	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)

	/* Test communication against a LASP enabled production account */

	// Account: 1939623
	// Policies: All policies set to "most-secure" (enabled:false)
	// Valid Token: "ffff-ffff-ffff-ffff"
	redirectCollector = "collector.newrelic.com"
	license1 = "5940d824e8244b19798fef5b6a70b8b73804484a"
	securityToken = "ffff-ffff-ffff-ffff"

	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)
	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)

	/* Test communication against LASP enabled staging accounts */

	// Account: 10002849
	// Policies: All policies set to "most-secure" (enabled:false)
	// Valid Token: "ffff-ffff-ffff-ffff"
	redirectCollector = "staging-collector.newrelic.com"
	license1 = "1cccc807e3eb81266a3f30d9a58cfbbe9d613049"
	securityToken = "ffff-ffff-ffff-ffff"

	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)
	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)

	// Account: 10005915
	// Policies: All policies set to "most-secure" (enabled:false), job_arguments has required:true
	// Valid Token: "ffff-ffff-ffff-ffff"
	redirectCollector = "staging-collector.newrelic.com"
	license1 = "20a5bbc045930ae7e15b530c8a9c6b7c5a918c4f"
	securityToken = "ffff-ffff-ffff-ffff"

	testExpectDaemonDisconnect(t, license1, redirectCollector, securityToken, supportedPolicies)
}
