# QGE (Quantun Game engine)
This is a mostly graphics engine, which includes following technologies:
  - Meshes:
    - FBX mesh loader;
    - Heightmap-based terrain loader.
  - GUI:
    - Self-written simple GUI using Direct2D.
  - Graphics effects:
    - SSAO (Screen Space Ambient Occlusion);
    - SSR (Screen Space Reflections);
    - OIT (Order Independent Transparency);
    - Voxel-based global illumination using light propagation volumes;
    - Voxel-based volumetric effects (clouds, fog);
    - SVO (Sparse Voxel Octree) realtime ray tracer;
    - Dynamic/static cube map reflections/refractions;
    - Reflective and refractive water surface;
    - Particle systems (rain, fire);
    - Environment map;
- Physics:
    - GPU fluid simulation using SPH (Smooth Particles Hydrodynamics);
    - GPU voxel-based body collision physics.
    
Engine is written in C++ and HLSL using DirectX 11 SDK and Autodesk FBX SDK.
