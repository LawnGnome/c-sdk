#include "newrelic.h"

Segment randomSegment(Transaction& txn) {
  return CustomSegment(txn, "Random", "Custom");
}
