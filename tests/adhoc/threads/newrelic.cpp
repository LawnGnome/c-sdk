#include <random>
#include <stdexcept>
#include <iostream>

#include "newrelic.h"
#include "random.h"

Segment randomSegment(Transaction& txn) {
  auto generator(defaultGenerator());
  std::uniform_int_distribution<int> distribution(0, 2);

  switch (distribution(generator)) {
    case 0:
      return CustomSegment(txn, "Random", "Custom");

    case 1:
      return DatastoreSegment(txn);

    case 2:
      return ExternalSegment(txn);

    default:
      throw std::logic_error("unexpected segment type");
  }
}
