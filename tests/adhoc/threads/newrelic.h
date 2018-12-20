/*
 * A handful of basic wrappers for C agent functions.
 */
#ifndef THREADS_NEWRELIC_HDR
#define THREADS_NEWRELIC_HDR

#include <stdexcept>
#include <string>

#include <boost/format.hpp>

#include "libnewrelic.h"

class NewRelicError : public std::runtime_error {
 public:
  NewRelicError(const std::string& what) : std::runtime_error(what) {}
};

class Config {
 public:
  Config(const std::string& appname, const std::string& licence)
      : config(newrelic_new_config(appname.c_str(), licence.c_str())) {
    if (nullptr == config) {
      throw NewRelicError("unable to create configuration");
    }
  }

  ~Config() { std::free(config); }

  newrelic_config_t* config;
};

class Application {
 public:
  Application(const Config& config, unsigned short timeoutMS = 0)
      : app(newrelic_create_app(config.config, timeoutMS)) {
    if (nullptr == app) {
      throw NewRelicError("unable to create application");
    }
  }

  ~Application() { newrelic_destroy_app(&app); }

  newrelic_app_t* app;
};

class Transaction {
 public:
  Transaction(Application& app, const std::string& name, bool web)
      : txn(web ? newrelic_start_web_transaction(app.app, name.c_str())
                : newrelic_start_non_web_transaction(app.app, name.c_str())) {
    if (nullptr == txn) {
      throw NewRelicError(boost::str(
          boost::format("unable to create transaction on application '%s'")
          % name));
    }
  }

  ~Transaction() { newrelic_end_transaction(&txn); }

  newrelic_txn_t* txn;
};

class Segment {
 public:
  Segment(Transaction& txn) : txn(txn.txn) {}
  virtual ~Segment() {}

 protected:
  newrelic_txn_t* txn;
};

class CustomSegment : public Segment {
 public:
  CustomSegment(Transaction& txn,
                const std::string& name,
                const std::string& category)
      : Segment(txn),
        segment(
            newrelic_start_segment(txn.txn, name.c_str(), category.c_str())) {
    if (nullptr == segment) {
      throw NewRelicError(boost::str(
          boost::format("unable to start custom segment on transaction %p with "
                        "name '%s' and category '%s'")
          % txn.txn % name % category));
    }
  }

  virtual ~CustomSegment() { newrelic_end_segment(txn, &segment); }

 private:
  newrelic_segment_t* segment;
};

class DatastoreSegment : public Segment {
 public:
  DatastoreSegment(Transaction& txn) : Segment(txn) {
    const newrelic_datastore_segment_params_t params = {
        .product = (char*)NEWRELIC_DATASTORE_MYSQL,
        .collection = (char*)"table",
        .operation = (char*)"select",
        .host = (char*)"localhost",
        .port_path_or_id = (char*)"3306",
        .database_name = (char*)"db",
        .query = (char*)"SELECT * FROM table WHERE foo = 'bar'",
    };

    segment = newrelic_start_datastore_segment(txn.txn, &params);
    if (nullptr == segment) {
      throw NewRelicError(boost::str(
          boost::format("unable to start datastore segment on transaction %p")
          % txn.txn));
    }
  }

  ~DatastoreSegment() { newrelic_end_datastore_segment(txn, &segment); }

 private:
  newrelic_datastore_segment_t* segment;
};

extern Segment randomSegment(Transaction& txn);

#endif /* THREADS_NEWRELIC_HDR */
