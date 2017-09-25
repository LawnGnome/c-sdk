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
git submodule update --init
```

Compile the agent
```
cd c-agent
make
```

Compile and start the daemon
```
make daemon
./php_agent/bin/daemon -f -logfile stdout -loglevel debug
```

Compile and run the test app
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
