#include "segment.h"

#include "util_logging.h"
#include "util_strings.h"

bool newrelic_validate_segment_param(const char* in, const char* name) {

  if (nr_strchr(in, '/')) {
  	if(NULL == name) {
			nrl_error(NRL_INSTRUMENT, "parameter cannot include a slash");
		}
	  else {
	  	nrl_error(NRL_INSTRUMENT, "%s cannot include a slash", name);
	  }
  	return false;
  }

  return true;
}
