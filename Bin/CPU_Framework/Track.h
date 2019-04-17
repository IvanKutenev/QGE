#ifndef _TRACK_H_
#define _TRACK_H_

#include "../Common/d3dUtil.h"
#include "..\Bin\GPU_Framework\MeshManager.h"
#include "..\Bin\GPU_Framework\GPUCollision.h"

class Track
{
public:
	Track::Track(MeshManager* pmeshmanager, std::map<BodyIndex, std::vector<MeshInstanceIndex>>&bodies, std::vector<MacroParams>&bodiesParams, ID3DX11EffectTechnique* drawtech);
};

#endif