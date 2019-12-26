
# Wavefront Converter

This tool converts files in the Wavefront 3D model format (.obj) into a custom binary format for
more efficient importing into a game engine. It is not designed to convert all information
stored in the source file. Currently it only reads position, normal and texture coordinate data.

The output files constitute one file per model from the source file, with a '.mdl' extension.
These will always include position data, and optionally normal and/or texture coordinate data
depending on which flags were supplied.

### Building and running

The code should build with any CMake-compatible toolchain. It has been tested only on
Windows, using both Visual Studio 2019 and CLion. The output is an executable that is
designed to be run on a command line.

### Flags

The command takes the form:

```
wavefrontconv FILE_NAME [FLAGS]
```

The recognised flags are:

| Long form | Short form | Description |
| --- | --- | --- |
| --help |  | Print usage information |
| --normals | -n | Include normal data in the exported files |
| --texcoords | -t | Include texture coordinate data in the exported files |

### Output format

The exported files contain binary data in the form below.

| Field | Type | Size (bytes) | Description |
| --- | --- | --- | --- |
| Version | Integer | 4 | Version number, currently only 1 |
| Normals included | Integer | 4 | Boolean (0 or 1), signals vertex data includes normals, after each position |
| Texture coordinates included | Integer | 4 | Boolean (0 or 1), signals vertex data includes texture coordinates, after each position/normal |
| Vertex count | Integer | 4 | 
| Vertex data | Float | X* | Data for unique vertex points - either 3, 5, 6 or 8 floats per vertex (depending on whether normal and/or texture coordinates are included) (4 bytes per float) |
| Triangle count | Integer | 4 | The number of triangles in this model |
| Index data | Float | Y* | Data for vertex indices for assembling triangles - 3 short integers per triangle (2 bytes per short) |

* X = N * S * 4 bytes, where N = number of unique vertices and S = the number of elements per vertex (3, 5, 6 or 8)
* Y = T * 6 bytes, where T = number of triangles in the model