package newrelic

import (
	"time"

	"newrelic/collector"
)

// A harvest trigger function. Each App has one of these, and it sends
// HarvestType messages to the given channel to initiate a harvest for that app.
// When an AppHarvest is closed, it sends a `true` event over the trigger
// function's cancel channel.
type HarvestTriggerFunc func(trigger chan HarvestType, cancel chan bool)

// Given a reply, determine whether the configuration requires the same reporting
// period across all data reporting.
func (reply *ConnectReply) isHarvestAll() bool {
	if reply != nil {
		dataMethods := reply.DataMethods

		return dataMethods.AllEqualTo(collector.DefaultReportPeriod)
	}

	// If a well-formed ConnectReply is not supplied, assume a default reporting
	// period across all data reporting.
	return true
}

func okParams(harvestsPerCycle int, cycleDuration time.Duration) bool {

	// These config values may be coming down from the collector over the wire,
	// so it seems  prudent to be particularly paranoid about them.
	if harvestsPerCycle == 0 {
		return false
	}

	if cycleDuration == 0 {
		return false
	}

	// Given that the trigger leverages modulo arithmetic to alternate harvesting
	// just the transaction events versus harvesting all data, the harvestsPerCycle
	// cannot be larger than the cycleDuration.
	if time.Duration(harvestsPerCycle) > cycleDuration {
		return false
	}

	// Perhaps the params are just asking for a default trigger
	if harvestsPerCycle == 1 && cycleDuration == 1*time.Minute {
		return false
	}

	return true
}

// A convenience function to create a harvest trigger function which triggers
// a harvest event of type t once every duration.
func triggerBuilder(t HarvestType, duration time.Duration) func(chan HarvestType, chan bool) {
	return func(trigger chan HarvestType, cancel chan bool) {
		ticker := time.NewTicker(duration)
		for {
			select {
			case <-ticker.C:
				trigger <- t
			case <-cancel:
				ticker.Stop()
				return
			}
		}
	}
}

// To create a group of goroutines that may be cancelled by sending a single
// message on a single channel, this function:
// - Creates a cancel channel for the goroutine function f.
// - Starts the goroutine.
// - Returns the newly-created cancel channel so that it may be added to a broadcast group.
func startGroupMember(f func(chan HarvestType, chan bool), trigger chan HarvestType) chan bool {
	cancel := make(chan bool)
	go f(trigger, cancel)
	return cancel
}

// In some cases, four different kinds of data are harvested at four different periods.  In such cases,
// build the comprehensive harvest trigger that adheres to such a configuration.
func customTriggerBuilder(reply *ConnectReply, reportPeriod int, units time.Duration) func(chan HarvestType, chan bool) {
	methods := reply.DataMethods

	defaultTrigger := triggerBuilder(HarvestDefaultData,
		time.Duration(reportPeriod)*units)
	analyticTrigger := triggerBuilder(HarvestTxnEvents,
		time.Duration(methods.GetValueOrDefault("AnalyticEventData"))*units)
	customTrigger := triggerBuilder(HarvestCustomEvents,
		time.Duration(methods.GetValueOrDefault("CustomEventData"))*units)
	errorTrigger := triggerBuilder(HarvestErrorEvents,
		time.Duration(methods.GetValueOrDefault("ErrorEventData"))*units)

	return func(trigger chan HarvestType, cancel chan bool) {
		broadcastGroup := make([]chan bool, 0)

		broadcastGroup = append(broadcastGroup, startGroupMember(defaultTrigger, trigger))
		broadcastGroup = append(broadcastGroup, startGroupMember(analyticTrigger, trigger))
		broadcastGroup = append(broadcastGroup, startGroupMember(customTrigger, trigger))
		broadcastGroup = append(broadcastGroup, startGroupMember(errorTrigger, trigger))

		// This function listens for the cancel message and then broadcasts it to all members
		// of the broadcastGroup.
		go func() {
			<-cancel
			for _, c := range broadcastGroup {
				c <- true
			}

		}()
	}
}

// A convenience function to create a harvest trigger function which triggers
// a HarvestAll event every cycleInMilliseconds, and triggers additional analytic
// event harvests, or HarvestEvents events, between those harvests.
//
// The harvestsPerCycle argument controls how many event harvests will occur
// per cycle for a cycle length of duration. If the parameters passed to this
// function are not well-formed, it returns a DefaultHarvestTrigger.
func createFastEventHarvestTrigger(harvestsPerCycle int, cycleDuration time.Duration) HarvestTriggerFunc {

	// If the parameters are the default or are not well-formed, just return the default trigger.
	if !okParams(harvestsPerCycle, cycleDuration) {
		return triggerBuilder(HarvestAll,
			time.Duration(collector.DefaultReportPeriod)*time.Second)
	}

	return func(trigger chan HarvestType, cancel chan bool) {
		count := 0
		interval := cycleDuration / time.Duration(harvestsPerCycle)
		for {
			ticker := time.NewTicker(interval)
			select {
			case <-ticker.C:
				if count == 0 {
					trigger <- HarvestAll
				} else {
					trigger <- HarvestTxnEvents
				}
				count = (count + 1) % harvestsPerCycle
			case <-cancel:
				ticker.Stop()
				return
			}
		}
	}
}

// customAppHarvestTrigger is a map of license keys that use a custom harvest
// trigger. The keys are hashed and base64-encoded license keys, and the values
// are the custom HarvestTriggerFunc functions that should be used.
//
// For the common case of a customer having a normal harvest cycle except for
// being granted additional HarvestEvents events, the createFastEventHarvestTrigger()
// function can be used to create a trigger function with the appropriate
// behaviour.
//
// Production accounts should only be added to this list with the approval of
// the PHP product manager and the Edge team.
//
// If you add or remove a key from this list, be sure to update the tests in
// harvest_trigger_test.go.
var customAppHarvestTrigger = map[string]HarvestTriggerFunc{
	// license "62f73a31951ead399a0c7298001d4118bb907aea" Production Account 24650
	"QpdPXlBTNAjbiiNNRBboT3WURS989QYFW6Zt9Idi7zk=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "9204290936137047b0d52f2850f2794c3396fc11" Production Account 366519
	"w0fCvRsCVM2t+PrBwfqfnm0Cu+2HxZZEmHZ4wOp1U+s=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "74490da46e436b2732cd299b81cf54942caba5ea" Production Account 672524
	"fxx8I4ZeCfERsfldnZ3PbGOnfd7enilHEiZMNskgD8o=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "89af2991f23abd82ad48295b6c6b419cb4d3cc6b" Production Account 261952
	"h5PoUfZ9lmoWDFL3b9zxQmTJKlJPWAJoCAmH2xl0QJY=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "d9a0a80c88b58b9c1237a3ef363a03d9acf7fcc1" Staging Account 432741
	"470j9XAtu/n1JEWJ8u8Bh7djIyoss+H0snuI7jlEeqg=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "4e8c0ad1c4e63058f78cea33765a9ef36004393b" Staging Account 204549
	"is5K3WHW24ZO13nES9rZdQzSa1VeVgKcGCft/BLmEKA=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "c2a090634623a5b9a307aba300894d0a1e8240e2" Staging Account 340824
	"eBZ1Or2LEpA9YaA7sAP73fDoL9pfJ8R5TP7ktAtYH6k=": createFastEventHarvestTrigger(3, 1*time.Minute),
	// license "7de210c476536509aa3028addc7034ebc1151fce" Production Account 990212
	"54lBhiJXGPkK0jordpTa3ikvG5sGdlGMxdlhXq3R+zU=": createFastEventHarvestTrigger(12, 1*time.Minute),
}

// This is separate purely for unit testing purposes. If Go could compare
// function pointers, then this wouldn't be needed.
func getCustomLicenseHarvestTrigger(key collector.LicenseKey) HarvestTriggerFunc {
	encoded := key.Sha256()
	return customAppHarvestTrigger[encoded]
}

// This function returns the harvest trigger function that should be used for this agent.  In priority order:
//   1. It checks a local whitelist and returns the harvest trigger appropriate to the LicenseKey.
//   2. It utilizes the ConnectReply to build custom triggers as specified by the New Relic server-side collector.
//   3. It creates a default harvest trigger, harvesting all data at the default period.
func getHarvestTrigger(key collector.LicenseKey, reply *ConnectReply) HarvestTriggerFunc {

	// TODO(tlc): In the very near future, in a separate PR, remove this function and the corresponding whitelist above.
	trigger := getCustomLicenseHarvestTrigger(key)

	// Build a trigger from the server-side collector configuration.
	if trigger == nil {
		if reply.isHarvestAll() {
			trigger = triggerBuilder(HarvestAll,
				time.Duration(collector.DefaultReportPeriod)*time.Second)
		} else {
			trigger = customTriggerBuilder(reply, collector.DefaultReportPeriod, time.Second)
		}
	}

	// Something in the server-side collector configuration was not well-formed.
	// Build a default HarvestAll trigger.
	if trigger == nil {
		trigger = triggerBuilder(HarvestAll,
			time.Duration(collector.DefaultReportPeriod)*time.Second)
	}

	return trigger
}
