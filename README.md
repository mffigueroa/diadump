diadump
=======

Just some code I was writing to learn more about parsing PDB files. You just give it an EXE file that has a PDB file associated with it, it will find the PDB file and parse it. Then it will go through all the functions mentioned in the PDB file, disassemble those, and try to display any variable accesses that are made that match up to variables mentioned in the PDB file.

Building
========

In order to build this code you need to get both LibXed2 and the DIA SDK. I've only tried building it in Visual Studio 2010 and 2012, but I don't see why other compilers wouldn't work unless these 2 libraries are incompatible with them.

I got Xed2 as part of Intel's Pin tool which can be found here: https://software.intel.com/en-us/articles/pin-a-dynamic-binary-instrumentation-tool

It can be found under Pin's directory in a folder called extras\xed2-intel64. I used the 64-bit version of this library, which is found in the directory I just mentioned.

Then DIA SDK can be found in the directory of any Visual Studio install under the directory appropriately called "DIA SDK".

Just add the lib and include directories of both of these libraries to the Library Directories and Include Directories settings respectively for your compiler and it should be ready to build. The project build settings in the Visual Studio project files are set to do a 64-bit build.
