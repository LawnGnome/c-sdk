package newrelic

import (
	"math/rand"
)

// CustomEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type CustomEvents struct {
	*analyticsEvents
}

// NewCustomEvents returns a new analytics event reservoir with capacity max.
func NewCustomEvents(max int) *CustomEvents {
	return &CustomEvents{newAnalyticsEvents(max)}
}

// AddEventFromData observes the occurance of a custom analytics
// event. If the reservoir is full, sampling occurs. Note: when
// sampling occurs, it is possible the new event may be discarded.
func (events *CustomEvents) AddEventFromData(data []byte) {
	stamp := eventStamp(rand.Float32())
	events.AddEvent(AnalyticsEvent{data: data, stamp: stamp})
}

// FailedHarvest is a callback invoked by the processor when an attempt to
// deliver events to the collector fails. After a failed delivery attempt,
// the events are merged into the upcoming harvest, possibly with random
// sampling.
func (events *CustomEvents) FailedHarvest(newHarvest *Harvest) {
	newHarvest.CustomEvents.MergeFailed(events.analyticsEvents)
}
