// automatically generated, do not modify

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type Metric struct {
	_tab flatbuffers.Table
}

func (rcv *Metric) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *Metric) Name() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *Metric) Data(obj *MetricData) *MetricData {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		x := o + rcv._tab.Pos
		if obj == nil {
			obj = new(MetricData)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func MetricStart(builder *flatbuffers.Builder) { builder.StartObject(2) }
func MetricAddName(builder *flatbuffers.Builder, name flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(0, flatbuffers.UOffsetT(name), 0)
}
func MetricAddData(builder *flatbuffers.Builder, data flatbuffers.UOffsetT) {
	builder.PrependStructSlot(1, flatbuffers.UOffsetT(data), 0)
}
func MetricEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT { return builder.EndObject() }
