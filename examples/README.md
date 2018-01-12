# C Agent Instrumentation Examples

The C Agent is an software development kit that allows users to manually instrument applications written in C 
or C++ for Linux 64-bit operating systems.  The source code in this `examples` directory provides examples of
how to instrument code.

All examples in this directory depend on two environment variables.  These are:

- `NR_APP_NAME`. A meaningful application name.
- `NR_LICENSE`. A valid New Relic license key.

Set these environment variables before executing the example programs.  Programs shall warn users if the
variables are unset.

The examples in this directory presume that the locations of libnewrelic.h and libnewrelic.a are one
level above this example directory.  If that is not the case, take care to set the following values
in `Makefile` appropriately:

```
LIBNEWRELIC_A_LOCATION = ../ # Location of libnewrelic.a
LIBNEWRELIC_H_LOCATION = ../ # Location of libnewrelic.h
```

## Error instrumentation

The source file `ex_notice_error.c` offers an example of how to instrument code using the function 
`newrelic_notice_error()`.  With this function, users may record transaction errors.  


To compile and execute 
this example, from the `examples` directory:

```
$ make ex_notice_error.out
$ ./ex_notice_error.out

```

After executing the example program, visit the New Relic Error Analytics dashboard for the account 
corresponding to the license key set as `NR_LICENSE`.  For each time that `ex_notice_error.out`
is executed an error with the message  "Meaningful error message" and error class "Error.class.supervalu"
is reported to New Relic and should be subsequently available at the aforementioned dashboard.