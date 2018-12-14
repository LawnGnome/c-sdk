package newrelic

import (
	"bytes"
	"container/heap"
	"encoding/json"
	"time"

	"newrelic/log"
)

// eventStamp allows for uniform random sampling of events.  When an event is
// created it is given an EventStamp.  Whenever an event pool is full and
// events need to be dropped, the events with the lowest stamps are dropped.
type eventStamp float32

func eventStampLess(a, b eventStamp) bool {
	return a < b
}

// AnalyticsEvent represents an analytics event reported by an agent.
type AnalyticsEvent struct {
	stamp eventStamp
	data  JSONString
}

// analyticsEventHeap implements a min-heap of analytics events according
// to their event stamps.
type analyticsEventHeap []AnalyticsEvent

// analyticsEvents represents a bounded collection of analytics events
// reported by agents. Then the collection is full, reservoir sampling
// with uniform distribution is used as the replacement strategy.
type analyticsEvents struct {
	numSeen        int
	events         *analyticsEventHeap // TODO(msl): Does this need to be a pointer?
	failedHarvests int
}

// NumSeen returns the total number of analytics events observed.
func (events *analyticsEvents) NumSeen() float64 {
	return float64(events.numSeen)
}

// NumSaved returns the number of analytics events in the reservoir.
func (events *analyticsEvents) NumSaved() float64 {
	return float64(len(*events.events))
}

func (h analyticsEventHeap) Len() int           { return len(h) }
func (h analyticsEventHeap) Less(i, j int) bool { return eventStampLess(h[i].stamp, h[j].stamp) }
func (h analyticsEventHeap) Swap(i, j int)      { h[i], h[j] = h[j], h[i] }

// Push appends x to the heap. This method should not be called
// directly because it does not maintain the min-heap property, and
// it does not enforce the maximum capacity. Use AddEvent instead.
func (h *analyticsEventHeap) Push(x interface{}) {
	// TODO(msl): We could ensure the maximum capacity is never exceeded by
	// re-slicing in place of append.
	*h = append(*h, x.(AnalyticsEvent))
}

// Pop removes and returns the analytics event with the least priority.
func (h *analyticsEventHeap) Pop() interface{} {
	old := *h
	n := len(old)
	x := old[n-1]
	*h = old[0 : n-1]
	return x
}

// newAnalyticsEvents returns a new event reservoir with capacity max.
func newAnalyticsEvents(max int) *analyticsEvents {
	h := make(analyticsEventHeap, 0, max)
	return &analyticsEvents{
		numSeen:        0,
		events:         &h,
		failedHarvests: 0,
	}
}

// AddEvent observes the occurence of an analytics event. If the
// reservoir is full, sampling occurs. Note, when sampling occurs, it
// is possible the event may be discarded instead of added.
func (events *analyticsEvents) AddEvent(e AnalyticsEvent) {
	events.numSeen++

	if len(*events.events) < cap(*events.events) {
		events.events.Push(e)
		if len(*events.events) == cap(*events.events) {
			// Delay heap initialization so that we can have deterministic
			// ordering for integration tests (the max is not being reached).
			heap.Init(events.events)
		}
		return
	}

	if eventStampLess(e.stamp, (*events.events)[0].stamp) {
		return
	}

	heap.Pop(events.events)
	heap.Push(events.events, e)
}

// MergeFailed merges the analytics events contained in other into
// events after a failed delivery attempt. If FailedEventsAttemptsLimit
// attempts have been made, the events in other are discarded. If events
// is full, reservoir sampling is performed.
func (events *analyticsEvents) MergeFailed(other *analyticsEvents) {
	fails := other.failedHarvests + 1
	if fails > FailedEventsAttemptsLimit {
		log.Debugf("discarding events: %d failed harvest attempts", fails)
		return
	}
	log.Debugf("merging events: %d failed harvest attempts", fails)
	events.failedHarvests = fails
	events.Merge(other)
}

// Merge merges the analytics events contained in other into events.
// If the combined number of events exceeds the maximum capacity of
// events, reservoir sampling with uniform distribution is performed.
func (events *analyticsEvents) Merge(other *analyticsEvents) {
	allSeen := events.numSeen + other.numSeen

	for _, e := range *other.events {
		events.AddEvent(e)
	}
	events.numSeen = allSeen
}

// CollectorJSON marshals events to JSON according to the schema expected
// by the collector.
func (events *analyticsEvents) CollectorJSON(id AgentRunID) ([]byte, error) {
	buf := &bytes.Buffer{}

	es := *events.events

	samplingData := struct {
		ReservoirSize int `json:"reservoir_size"`
		EventsSeen    int `json:"events_seen"`
	}{
		ReservoirSize: cap(es),
		EventsSeen:    events.numSeen,
	}

	estimate := len(es) * 128
	buf.Grow(estimate)
	buf.WriteByte('[')

	enc := json.NewEncoder(buf)
	if err := enc.Encode(id); err != nil {
		return nil, err
	}
	// replace trailing newline
	buf.Bytes()[buf.Len()-1] = ','

	if err := enc.Encode(samplingData); err != nil {
		return nil, err
	}

	buf.Bytes()[buf.Len()-1] = ','

	buf.WriteByte('[')
	for i := 0; i < len(es); i++ {
		if i > 0 {
			buf.WriteByte(',')
		}
		buf.Write(es[i].data)
	}
	buf.WriteByte(']')
	buf.WriteByte(']')

	return buf.Bytes(), nil
}

// Empty returns true if the collection is empty.
func (events *analyticsEvents) Empty() bool {
	return 0 == events.events.Len()
}

// Data marshals the collection to JSON according to the schema expected
// by the collector.
func (events *analyticsEvents) Data(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return events.CollectorJSON(id)
}

// Audit marshals the collection to JSON according to the schema
// expected by the audit log. For analytics events, the audit schema is
// the same as the schema expected by the collector.
func (events *analyticsEvents) Audit(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return nil, nil
}
