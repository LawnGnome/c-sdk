// automatically generated, do not modify

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type Trace struct {
	_tab flatbuffers.Table
}

func (rcv *Trace) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *Trace) Timestamp() float64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetFloat64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Trace) Duration() float64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.GetFloat64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Trace) Guid() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(8))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *Trace) ForcePersist() byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(10))
	if o != 0 {
		return rcv._tab.GetByte(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Trace) Data() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(12))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func TraceStart(builder *flatbuffers.Builder) { builder.StartObject(5) }
func TraceAddTimestamp(builder *flatbuffers.Builder, timestamp float64) {
	builder.PrependFloat64Slot(0, timestamp, 0)
}
func TraceAddDuration(builder *flatbuffers.Builder, duration float64) {
	builder.PrependFloat64Slot(1, duration, 0)
}
func TraceAddGuid(builder *flatbuffers.Builder, guid flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(2, flatbuffers.UOffsetT(guid), 0)
}
func TraceAddForcePersist(builder *flatbuffers.Builder, forcePersist byte) {
	builder.PrependByteSlot(3, forcePersist, 0)
}
func TraceAddData(builder *flatbuffers.Builder, data flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(4, flatbuffers.UOffsetT(data), 0)
}
func TraceEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT { return builder.EndObject() }
