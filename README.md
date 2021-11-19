# Tools for LinuxPPS

You can read more about LinuxPPS [here](http://linuxpps.org)

## Building

```sh
make
```

## Cross-compiling

Example for Linux ARM

```sh
export CC=<CrossCompilerBinPath>/arm-linux-gnueabihf-gcc
export SYSROOT=<PathToSysroot>
make
```
