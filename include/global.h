#ifndef LIBNEWRELIC_GLOBAL_H
#define LIBNEWRELIC_GLOBAL_H

bool newrelic_do_init(const char* daemon_socket, int time_limit_ms);

bool newrelic_ensure_init(void);

const char* newrelic_resolve_daemon_socket(const char* user_path);

void newrelic_shutdown(void);

#endif /* LIBNEWRELIC_GLOBAL_H */
