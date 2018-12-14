#include "nr_axiom.h"

#include <stddef.h>

#include "nr_datastore_instance.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"

nr_datastore_instance_t* nr_datastore_instance_create(
    const char* host,
    const char* port_path_or_id,
    const char* database_name) {
  nr_datastore_instance_t* instance;

  instance
      = (nr_datastore_instance_t*)nr_zalloc(sizeof(nr_datastore_instance_t));
  nr_datastore_instance_set_host(instance, host);
  nr_datastore_instance_set_port_path_or_id(instance, port_path_or_id);
  nr_datastore_instance_set_database_name(instance, database_name);

  return instance;
}

void nr_datastore_instance_destroy(nr_datastore_instance_t** instance_ptr) {
  if ((NULL == instance_ptr) || (NULL == *instance_ptr)) {
    return;
  }

  nr_free((*instance_ptr)->host);
  nr_free((*instance_ptr)->port_path_or_id);
  nr_free((*instance_ptr)->database_name);
  nr_realfree((void**)instance_ptr);
}

/*
 * See:
 * https://source.datanerd.us/agents/agent-specs/blob/master/Datastore-Metrics-PORTED.md#datastore-instance-identifier
 */
int nr_datastore_instance_is_localhost(const char* host) {
  if (0 == nr_strcmp(host, "localhost")) {
    return 1;
  } else if (0 == nr_strcmp(host, "127.0.0.1")) {
    return 1;
  } else if (0 == nr_strcmp(host, "0.0.0.0")) {
    return 1;
  } else if (0 == nr_strcmp(host, "0:0:0:0:0:0:0:1")) {
    return 1;
  } else if (0 == nr_strcmp(host, "::1")) {
    return 1;
  } else if (0 == nr_strcmp(host, "0:0:0:0:0:0:0:0")) {
    return 1;
  } else if (0 == nr_strcmp(host, "::")) {
    return 1;
  }

  return 0;
}

const char* nr_datastore_instance_get_host(nr_datastore_instance_t* instance) {
  if (NULL == instance) {
    return NULL;
  }

  return instance->host;
}

const char* nr_datastore_instance_get_port_path_or_id(
    nr_datastore_instance_t* instance) {
  if (NULL == instance) {
    return NULL;
  }

  return instance->port_path_or_id;
}

const char* nr_datastore_instance_get_database_name(
    nr_datastore_instance_t* instance) {
  if (NULL == instance) {
    return NULL;
  }

  return instance->database_name;
}

void nr_datastore_instance_set_host(nr_datastore_instance_t* instance,
                                    const char* host) {
  if (NULL == instance) {
    return;
  }

  nr_free(instance->host);
  if (nr_datastore_instance_is_localhost(host)) {
    instance->host = nr_system_get_hostname();
  } else {
    if ((NULL == host) || (nr_strlen(host) <= 0)) {
      instance->host = nr_strdup("unknown");
    } else {
      instance->host = nr_strdup(host);
    }
  }
}

void nr_datastore_instance_set_port_path_or_id(
    nr_datastore_instance_t* instance,
    const char* port_path_or_id) {
  if (NULL == instance) {
    return;
  }

  nr_free(instance->port_path_or_id);
  if ((NULL == port_path_or_id) || (nr_strlen(port_path_or_id) <= 0)) {
    instance->port_path_or_id = nr_strdup("unknown");
  } else {
    instance->port_path_or_id = nr_strdup(port_path_or_id);
  }
}

void nr_datastore_instance_set_database_name(nr_datastore_instance_t* instance,
                                             const char* database_name) {
  if (NULL == instance) {
    return;
  }

  nr_free(instance->database_name);
  if ((NULL == database_name) || (nr_strlen(database_name) <= 0)) {
    instance->database_name = nr_strdup("unknown");
  } else {
    instance->database_name = nr_strdup(database_name);
  }
}
