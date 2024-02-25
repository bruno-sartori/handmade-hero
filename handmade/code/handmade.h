#if !defined(HANDMADE_H)

#include "handmade_platform.h"

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

//
//
//

struct memory_arena {
  memory_index Size;
  uint8 *Base;
  memory_index Used;
};

internal void InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base) {
  Arena->Size = Size;
  Arena->Base = Base;
  Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count) * sizeof(type))
void * PushSize_(memory_arena *Arena, memory_index Size) {
  Assert((Arena->Used + Size) <= Arena->Size);
  void *Result = Arena->Base + Arena->Used;
  Arena->Used += Size;

  return (Result);
}

#include "handmade_math.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct world {
  tile_map *TileMap;
};

struct loaded_bitmap {
  int32 Width;
  int32 Height;
  uint32 *Pixels;
};

struct hero_bitmaps {
  uint32 AlignX;
  uint32 AlignY;
  loaded_bitmap Head;
  loaded_bitmap Cape;
  loaded_bitmap Torso;
};

struct high_entity {
  bool32 Exists;
  v2 P; // NOTE: Position relative to the camera!
  v2 dP; // velocity
  uint32 FacingDirection;
};

struct low_entity {
};

struct dormant_entity {
  tile_map_position P;
  real32 Width;
  real32 Height;
};

enum entity_residence {
  EntityResidence_Nonexistent,
  EntityResidence_Dormant,
  EntityResidence_Low,
  EntityResidence_High,
};

struct entity {
  uint32 Residence;
  dormant_entity *Dormant;
  low_entity *Low;
  high_entity *High;
};

struct game_state {
  memory_arena WorldArena;
  world *World;

  // TODO: Should we allow split-screen
  uint32 CameraFollowingEntityIndex;
  tile_map_position CameraP;

  uint32 PlayerIndexForController[ArrayCount(((game_input *)0)->Controllers)];
  uint32 EntityCount;
  entity_residence EntityResidence[256];

  high_entity HighEntities[256];
  low_entity LowEntities[256];
  dormant_entity DormantEntities[256];

  loaded_bitmap Backdrop;
  hero_bitmaps HeroBitmaps[4];

};

#define HANDMADE_H
#endif
