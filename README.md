# Sch

Sch is a C++ library for transistor-level netlist processing and physical cell layout. Given a MOS subcircuit and a technology description, it can place transistors in NMOS and PMOS rows, route their connections, and draw the result as a physical layout.

## Cell layout flow

1. **Prepare the netlist.** [`Subckt`](sch/Subckt.h) stores MOS devices, nets, external ports, and child instances. It can combine parallel devices, split devices to fit technology width limits, and normalize transistor connectivity for comparison.
2. **Place the transistors.** [`CellPlacement::solve()`](sch/CellPlacer.cpp) searches device orders and orientations in separate NMOS and PMOS rows. It scores placements by row width, estimated horizontal net span, route overlap, and alignment across the rows.
3. **Route the cell.** [`CellRouter::solve()`](sch/CellRouter.cpp) assigns pin offsets and route order from spacing constraints. It can split routes to break cycles and select lower routing layers where pin geometry permits. It returns false if route or pin constraint cycles remain unresolved.
4. **Draw the layout.** [`drawCell()`](sch/Draw.cpp) assembles the routed geometry in a `phy::Layout`. The drawing helpers generate diffusion, gates, contacts, vias, and wire geometry from technology rules.

## Dependencies

```
sudo apt install opencl-headers ocl-icd-opencl-dev -y
```

## Build

Run these commands from the Sch directory. The default target builds `libsch.a` from the C++ sources under `sch/`. Use a C++20 compiler and GNU Make.

```sh
make
```

The [Makefile](Makefile) uses the sibling `interpret_phy`, `phy`, `common`, and `gdstk` directories. It uses `python3` and `python3-config` to get the Python version and compiler include flags. The OpenCL C++ bindings target OpenCL 1.2 with exceptions enabled.

The build embeds [`cl/placer.cpp`](cl/placer.cpp) as a string in the generated `sch/Kernel.h`. Edit the kernel source to change the embedded code.

## Source map

| Path | Contents |
| --- | --- |
| [`sch/Subckt.*`](sch/Subckt.h) and [`sch/Segment.*`](sch/Segment.h) | Transistor netlists and segments of devices within a netlist. |
| [`sch/Isomorph.*`](sch/Isomorph.h) | Graph labeling used to put transistor networks in canonical form. |
| [`sch/CellPlacer.*`](sch/CellPlacer.h) | Transistor ordering, orientation, and horizontal placement within a cell. |
| [`sch/CellRouter.*`](sch/CellRouter.h) and [`sch/Constraint.*`](sch/Constraint.h) | Cell routing and its spacing and ordering constraints. |
| [`sch/Draw.*`](sch/Draw.h) | Physical layout geometry for devices, contacts, and routes. |
| [`sch/Placer.*`](sch/Placer.h), [`sch/Kernel.h`](sch/Kernel.h), and [`cl/placer.cpp`](cl/placer.cpp) | Placer and kernel sources. |
| [`sch/Tapeout.*`](sch/Tapeout.h) | Tapeout module. |
| [`sch/bitset.*`](sch/bitset.h) | Bitset support used by the router to track ordering relationships. |

## Tests

The [`tests/`](tests/) directory covers [cell placement](tests/CellPlacer.cpp), [graph isomorphism](tests/Isomorph.cpp), and the [placer](tests/Placer.cpp).

```sh
make tests
./test
```

Build GoogleTest before you build the tests. The Makefile looks for its source and built library under `../../googletest` by default. To use a different checkout, run `make tests GTEST=/path/to/googletest`. The test linker uses the built sibling libraries, GoogleTest, gdstk, Qhull, and zlib. On Linux it links `libOpenCL`. On macOS it links the OpenCL framework. The macOS build uses Homebrew paths for Qhull and OpenCL headers and sets a minimum deployment target of macOS 26.0.

## Coverage

Install `lcov` and `genhtml`, then run:

```sh
make coverage
```

This target cleans the build, rebuilds with coverage instrumentation, and runs the tests. It continues to generate the report if tests fail. Open `coverage_report/index.html` to view the report.

## License

Licensed by Broccoli, LLC under GNU GPL v3.

Written by Ned Bingham.
Copyright © 2024 Broccoli, LLC.

Bok is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Bok is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License may be found in COPYRIGHT.
Otherwise, see <https://www.gnu.org/licenses/>.

