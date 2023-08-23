#include "ply_reader.hpp"

namespace ply
{

const char *elem_names[] = { /* list of the kinds of elements in the user's object */
  "vertex",
  "face"
};

PlyProperty vert_props[] = {
  /* list of property information for a vertex */
  { "x", Float32, Float32, offsetof(Vertex, x), 0, 0, 0, 0 },
  { "y", Float32, Float32, offsetof(Vertex, y), 0, 0, 0, 0 },
  { "z", Float32, Float32, offsetof(Vertex, z), 0, 0, 0, 0 },
  { "nx", Float32, Float32, offsetof(Vertex, nx), 0, 0, 0, 0 },
  { "ny", Float32, Float32, offsetof(Vertex, ny), 0, 0, 0, 0 },
  { "nz", Float32, Float32, offsetof(Vertex, nz), 0, 0, 0, 0 },
};

PlyProperty face_props[] = {
  /* list of property information for a face */
  { "vertex_indices", Int32, Int32, offsetof(Face, verts), 1, Uint8, Uint8, offsetof(Face, nverts) },
};

void read_ply_file(const char *path, std::vector<Vertex> &vertices, std::vector<Face> &faces)
{
  int i, j;
  int elem_count;
  char *elem_name;

  /*** Read in the original PLY object ***/

  FILE *file = fopen(path, "rb");
  ;// VK_GRAPHICS_BASIC_ROOT "/resources/scenes/point_cloud/bunny_cornell/data/bun000.ply"
  auto in_ply = read_ply(file);

  /* examine each element type that is in the file (vertex, face) */

  for (i = 0; i < in_ply->num_elem_types; i++)
  {

    /* prepare to read the i'th list of elements */
    elem_name = setup_element_read_ply(in_ply, i, &elem_count);

    if (equal_strings((char*)"vertex", elem_name))
    {

      /* create a vertex list to hold all the vertices */
      vertices.reserve(elem_count);
      auto nverts = elem_count;

      /* set up for getting vertex elements */
      /* (we want x,y,z) */

      setup_property_ply(in_ply, &vert_props[0]);
      setup_property_ply(in_ply, &vert_props[1]);
      setup_property_ply(in_ply, &vert_props[2]);

      /* we also want normal information if it is there (nx,ny,nz) */

      for (j = 0; j < in_ply->elems[i]->nprops; j++)
      {
        PlyProperty *prop;
        prop = in_ply->elems[i]->props[j];
        if (equal_strings((char *)"nx", (char*)prop->name))
        {
          setup_property_ply(in_ply, &vert_props[3]);
          auto has_nx = 1;
        }
        if (equal_strings((char *)"ny", (char *)prop->name))
        {
          setup_property_ply(in_ply, &vert_props[4]);
          auto has_ny = 1;
        }
        if (equal_strings((char *)"nz", (char *)prop->name))
        {
          setup_property_ply(in_ply, &vert_props[5]);
          auto has_nz = 1;
        }
      }

      /* also grab anything else that we don't need to know about */

      auto vert_other = get_other_properties_ply(in_ply,
        offsetof(Vertex, other_props));

      /* grab the vertex elements and store them in our list */

      for (j = 0; j < elem_count; j++)
      {
        vertices.push_back({});
        get_element_ply(in_ply, (void *)&(vertices[j]));
      }
    }
    else if (equal_strings((char*)"face", elem_name))
    {

      /* create a list to hold all the face elements */
      faces.reserve(elem_count);
      auto nfaces = elem_count;

      /* set up for getting face elements */
      /* (all we need are vertex indices) */

      setup_property_ply(in_ply, &face_props[0]);
      auto face_other = get_other_properties_ply(in_ply,
        offsetof(Face, other_props));

      /* grab all the face elements and place them in our list */

      for (j = 0; j < elem_count; j++)
      {
        faces.push_back({});
        get_element_ply(in_ply, (void *)&(faces[j]));
      }
    }
    else /* all non-vertex and non-face elements are grabbed here */
      get_other_element_ply(in_ply);
  }

  /* close the file */
  /* (we won't free up the memory for in_ply because we will use it */
  /*  to help describe the file that we will write out) */

  close_ply(in_ply);
}

} // namespace ply
