# Task Monitor Reader
Simple reader application for taskmonitor

## Getting started
This is a host tool to connect and read data stream from taskmonitor service.
The output is serialised in JSON data format to be used in data visualization tools.

## Download
`# git clone --recurse-submodules https://gitlab.com/taskmonitor/tkm-reader.git`

## Dependencies

TKM-Reader depends on the following libraries

| Library | Reference | Info |
| ------ | ------ | ------ |
| jsoncpp | https://github.com/open-source-parsers/jsoncpp | Emit Json output |
| protobuf | https://developers.google.com/protocol-buffers | Data serialization |
| libsystemd | https://github.com/systemd/systemd/tree/main/src/libsystemd | Optional if WITH_SYSTEMD is ON 

## Build
### Compile options

| Option | Default | Info |
| ------ | ------ | ------ |
| WITH_SYSTEMD | ON | Enable systemd service and watchdog support |

### Local Build
`mkdir build && cd build && cmake .. && make `