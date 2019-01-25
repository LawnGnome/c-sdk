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
				// Send a message back to the cancel channel confirming that the ticker
				// has been stopped.
				cancel <- true
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

		// This function listens for the cancel message and then broadcasts it to
		// all members of the broadcastGroup.
		go func() {
			<-cancel
			for _, c := range broadcastGroup {
				c <- true
				// As we need to send a confirmation that all trigger functions have
				// been cancelled, we'll wait for this function to confirm that the
				// cancellation has been processed.
				<-c
			}

			// Send a confirmation that the cancellation has been processed, since we
			// know from the loop above that all functions in the broadcast group have
			// been cancelled.
			cancel <- true
		}()
	}
}

// This function returns the harvest trigger function that should be used for this agent.  In priority order:
//   1. Either it uses the ConnectReply to build custom triggers as specified by the New Relic server-side collector.
//   2. Or it creates a default harvest trigger, harvesting all data at the default period.
func getHarvestTrigger(key collector.LicenseKey, reply *ConnectReply) HarvestTriggerFunc {

	var trigger func(chan HarvestType, chan bool)

	// Build a trigger from the server-side collector configuration.
	if reply.isHarvestAll() {
		trigger = triggerBuilder(HarvestAll,
			time.Duration(collector.DefaultReportPeriod)*time.Second)
	} else {
		trigger = customTriggerBuilder(reply, collector.DefaultReportPeriod, time.Second)
	}

	// Something in the server-side collector configuration was not well-formed.
	// Build a default HarvestAll trigger.
	if trigger == nil {
		trigger = triggerBuilder(HarvestAll,
			time.Duration(collector.DefaultReportPeriod)*time.Second)
	}

	return trigger
}
