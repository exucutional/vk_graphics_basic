#pragma once

#include <stdio.h>
#include <math.h>
#include <cstring>
#include <memory>
#include <vector>
#include "ply.h"

namespace ply
{

typedef struct Vertex
{
  float x, y, z;
  float nx, ny, nz;
  void *other_props; /* other properties */
} Vertex;

typedef struct Face
{
  unsigned char nverts; /* number of vertex indices in list */
  int *verts; /* vertex index list */
  void *other_props; /* other properties */
} Face;

void read_ply_file(const char* path, std::vector<Vertex>&, std::vector<Face>&);

} // namespace ply