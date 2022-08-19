# CSLVM

CSLVM is an implementation of the SLVM, created by [@Its-Jakey](https://github.com/Its-Jakey).

See the specification [here](https://github.com/Its-Jakey/SCPP/blob/master/SLVM%20Bytecode.md).

## Why?

- PySCPP's SLVM is a bit slow
- SCPP's SLVM cannot be accessed without compiling source code
- the SLVM in scratch is even slower than the one in PySCPP

## building

You will need to install the following dependencies:

- [glfw](https://www.glfw.org/download.html)
- [CMake](http://www.cmake.org/download/index.html)

## usage

    CSLVM [path to file]

There are a few optional flags:

- `-g`, `--graph`: Create a window for graphics.
- `-d`, `--dump`: Dump the memory to a file when the program exits.
