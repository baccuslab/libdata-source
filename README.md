# libdata-source

Qt library for controlling and interacting with data sources in the Baccus Lab.

(C) 2016-2017 Benjamin Naecker bnaecker@stanford.edu

## Overview

This dynamic library is intended for use in the recording software
ecosystem in the Baccus Lab at Stanford University. It is intended to provide
a consistent interface to any source of experimental data in the lab. This
is done via a `BaseSource` class, which defines a consistent API that all data
sources follow. Client code creates instances of one of these base classes, 
which allow interacting with either MCS array devices, via the `McsSource` class,
HiDens array devices, via the `HidensSource` class, or previously recorded 
data files, via the `FileSource` class.

## Requirements and building

The library requires
 
- Qt5 or greater
- C++11-compatible compiler
- Armadillo linear algebra library (for managing data from the devices)
- Doxygen (for building documentation)

To build the library, use

	$ qmake && make
	$ make doc # For building documentation

