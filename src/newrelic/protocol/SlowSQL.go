// automatically generated, do not modify

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type SlowSQL struct {
	_tab flatbuffers.Table
}

func (rcv *SlowSQL) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *SlowSQL) Id() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *SlowSQL) Count() int32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.GetInt32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *SlowSQL) TotalMicros() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(8))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *SlowSQL) MinMicros() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(10))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *SlowSQL) MaxMicros() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(12))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *SlowSQL) Metric() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(14))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *SlowSQL) Query() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(16))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *SlowSQL) Params() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(18))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func SlowSQLStart(builder *flatbuffers.Builder)                 { builder.StartObject(8) }
func SlowSQLAddId(builder *flatbuffers.Builder, id uint32)      { builder.PrependUint32Slot(0, id, 0) }
func SlowSQLAddCount(builder *flatbuffers.Builder, count int32) { builder.PrependInt32Slot(1, count, 0) }
func SlowSQLAddTotalMicros(builder *flatbuffers.Builder, totalMicros uint64) {
	builder.PrependUint64Slot(2, totalMicros, 0)
}
func SlowSQLAddMinMicros(builder *flatbuffers.Builder, minMicros uint64) {
	builder.PrependUint64Slot(3, minMicros, 0)
}
func SlowSQLAddMaxMicros(builder *flatbuffers.Builder, maxMicros uint64) {
	builder.PrependUint64Slot(4, maxMicros, 0)
}
func SlowSQLAddMetric(builder *flatbuffers.Builder, metric flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(5, flatbuffers.UOffsetT(metric), 0)
}
func SlowSQLAddQuery(builder *flatbuffers.Builder, query flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(6, flatbuffers.UOffsetT(query), 0)
}
func SlowSQLAddParams(builder *flatbuffers.Builder, params flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(7, flatbuffers.UOffsetT(params), 0)
}
func SlowSQLEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT { return builder.EndObject() }
