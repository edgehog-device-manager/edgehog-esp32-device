# ESP-IDF pytest-embedded

1. how `pytest_embedded_serial_esp` auto-detect chip target and port
2. how `pytest_embedded_idf` auto flash the app into the target chip

## Prerequisites

1. Connect to the target chips
2. Install following packages
   - `pytest_embedded`
   - `pytest_embedded_serial_esp`
   - `pytest_embedded_idf`
3. cd into the app folder
4. run `idf.py build` under the apps you want to test

##idf
```shell
$ pytest --embedded-services=esp,idf
```

The ... idf app qemu flash image automatically and run this with qemu.

## Prerequisites

1. Prepare QEMU program which supports xtensa, name it `qemu-system-xtensa` and add its parent directory into `$PATH`
2. Install following packages
   - `pytest_embedded`
   - `pytest_embedded_idf`
   - `pytest_embedded_qemu`
   - `esptool` (for sending commands to QEMU via socket)
3. cd into the app folder
4. run `idf.py build` under the apps you want to test

## Test Steps

## qemu
```shell
$ pytest --embedded-services=idf,qemu
```

QEMU flash image would be created automatically if not exists
