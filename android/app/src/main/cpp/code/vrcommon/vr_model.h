/* A runtime-supplied controller model (binary glTF) reduced to what one unlit pass can draw: static geometry,
 * a matrix per node, and headlight colors per vertex. No engine, Vulkan or OpenXR dependencies. */
#ifndef VR_MODEL_H
#define VR_MODEL_H
#include <stddef.h>

#define VR_MODEL_MAX_TRIANGLES 65536
#define VR_MODEL_MAX_NODES 1024
#define VR_MODEL_NAME_SIZE 64

/* An OpenXR pose: position in meters, orientation x, y, z, w. */
typedef struct {
	float position[3], orientation[4];
} vrModelPose_t;

typedef struct {
	vrModelPose_t pose; /* in the parent node's space; replaces the node's own transform */
	int visible;
} vrModelNodeState_t;

typedef struct {
	int parent, mesh; /* -1 for none; a parent always precedes its children */
	float local[16];  /* column-major rest transform */
	char name[VR_MODEL_NAME_SIZE];
} vrModelNode_t;

typedef struct {
	int vertexCount, indexCount, material;
	float *positions, *normals, *texCoords; /* 3, 3 and 2 floats per vertex */
	unsigned *indices;
} vrModelPrimitive_t;

typedef struct {
	int firstPrimitive, primitiveCount;
} vrModelMesh_t;

typedef struct {
	float color[4];
	int image; /* -1 for none */
	int doubleSided;
} vrModelMaterial_t;

/* pixels is the caller's decoded RGBA, from the model's allocator, released with the model. */
typedef struct {
	const unsigned char *data;
	int size, jpeg;
	unsigned char *pixels;
	int width, height;
} vrModelImage_t;

typedef struct {
	int nodeCount, meshCount, primitiveCount, materialCount, imageCount, triangleCount;
	vrModelNode_t *nodes;
	vrModelMesh_t *meshes;
	vrModelPrimitive_t *primitives;
	vrModelMaterial_t *materials; /* the last one is the default material */
	vrModelImage_t *images;
	void *block; /* the one allocation everything above lives in */
} vrModel_t;

typedef void *( *vrModelAlloc_t )( size_t size );
typedef void ( *vrModelFree_t )( void *memory );

/* Returns 0 for malformed, oversized or unsupported input and leaves nothing allocated. Retains nothing of glb. */
int VR_ModelParse( const void *glb, size_t size, vrModelAlloc_t alloc, vrModelFree_t release, vrModel_t *model );
void VR_ModelFree( vrModel_t *model, vrModelFree_t release );
/* The pixel size an encoded image declares, read from its header without decoding it. 0 when there is none to read. */
int VR_ModelImageSize( const vrModelImage_t *image, int *width, int *height );
/* map[i] becomes the node named names[i], or -1. */
void VR_ModelBindNodes( const vrModel_t *model, const char ( *names )[VR_MODEL_NAME_SIZE], int count, int *map );
/* world gets 16 floats per node with the root pose applied, visible one byte per node; states NULL means the rest pose. */
void VR_ModelPose( const vrModel_t *model, const vrModelPose_t *root, const vrModelNodeState_t *states,
				   const int *map, int count, float *world, unsigned char *visible );
/* The static half of drawing, for a GPU buffer: a primitive after another, each vertexCount positions (4 floats,
 * w 1), then vertexCount texture coordinates (2 floats), then indexCount 32-bit indices. offsets receives each
 * primitive's start. Returns the size; out and offsets may be NULL to measure. */
#define VR_MODEL_PACK_ST( vertexCount ) ( (unsigned)( vertexCount ) * 16 )
#define VR_MODEL_PACK_INDEX( vertexCount ) ( (unsigned)( vertexCount ) * 24 )
size_t VR_ModelPack( const vrModel_t *model, void *out, unsigned *offsets );
/* Headlight colors for one posed primitive, 4 bytes a vertex; the back of a single-sided material stays at the ambient floor. */
void VR_ModelShade( const vrModel_t *model, int primitive, const float world[16], const float eye[3],
					unsigned char *rgba );
#endif
