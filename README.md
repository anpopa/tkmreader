# Task Monitor Reader
Simple reader application for **TaskMonitor**

## Getting started
This is a host tool to connect and read data stream from taskmonitor service.
The output is serialized in JSON data format to be used in data visualization tools.

## Download
`# git clone --recurse-submodules https://gitlab.com/taskmonitor/tkm-reader.git`

## Dependencies

TKM-Reader depends on the following libraries

| Library | Reference | Info |
| ------ | ------ | ------ |
| sqlite3 | https://www.sqlite.org/index.html | Output sqlite3 database |
| jsoncpp | https://github.com/open-source-parsers/jsoncpp | Emit Json output |
| protobuf | https://developers.google.com/protocol-buffers | Data serialization |

## Build
### Compile options

| Option | Default | Info |
| ------ | ------ | ------ |
| WITH_SYSLOG | OFF | Print log output to syslog instead of stdout |

### Local Build
`mkdir build && cd build && cmake .. && make `
