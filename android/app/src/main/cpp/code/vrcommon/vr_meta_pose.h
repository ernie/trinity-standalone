/* Meta's controller models (XR_FB_render_model): the input nodes its own loader animates and the pose each takes
 * for a hand's input, from fixed keyframes of the model's one clip. No engine or OpenXR dependencies. */
#ifndef VR_META_POSE_H
#define VR_META_POSE_H
#include "vr_model.h"

enum {
	VR_META_BUTTON_AX,
	VR_META_BUTTON_BY,
	VR_META_BUTTON_MENU,
	VR_META_TRIGGER_FRONT,
	VR_META_TRIGGER_GRIP,
	VR_META_THUMBSTICK,
	VR_META_INPUT_COUNT
};

typedef struct {
	int buttonAX, buttonBY, menu; /* down */
	float trigger, grip;		  /* 0 to 1 */
	float stick[2];				  /* x right, y forward, each -1 to 1 */
} vrMetaInput_t;

/* map[i] becomes the node whose name carries the input's name, or -1. Returns how many were found. */
int VR_MetaBindInputs( const vrModel_t *model, int map[VR_META_INPUT_COUNT] );
/* The input nodes' states for one hand's input: a button rests or sits at its pressed keyframe, a trigger
 * leans toward its by its value, the stick toward the two nearest of its eight directions by how far it is pushed. */
void VR_MetaPoseInputs( const vrModel_t *model, const int map[VR_META_INPUT_COUNT], const vrMetaInput_t *input,
						vrModelNodeState_t states[VR_META_INPUT_COUNT] );
#endif
