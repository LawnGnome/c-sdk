# New Relic C Agent

### Description
Generic library to communicate with New Relic.

### Requirements

* libpcre 8.13+
* golang 1.4+

### Getting started

Clone the agent
```
git clone git@source.datanerd.us:c-agent/c-agent.git
```

Pull in dependencies
```
cd /path/to/checked-out/c-agent
git submodule update --init --recursive
```

Compile the agent
```
make
```

Compile and run unit tests
```
make all
make run_tests
```

Compile and start the daemon
```
make daemon
./php_agent/bin/daemon -f --logfile stdout --loglevel debug
```

Compile and run the test app, (the `test_app` requires a running daemon to work properly -- see above)

```
make test_app
./test_app
```

Check out the data in the [PHP test account](https://staging.newrelic.com/accounts/432507/applications/)!

### What do we give customers?

```
libnewrelic
|-- bin
|   |-- newrelic-daemon
|-- libnewrelic.h
|-- libnewrelic.a
|-- GUIDE.md
|-- LICENSE.txt
```
