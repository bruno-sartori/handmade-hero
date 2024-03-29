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

#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"

struct loaded_bitmap {
  int32 Width;
  int32 Height;
  uint32 *Pixels;
};

struct hero_bitmaps {
  v2 Align;
  loaded_bitmap Head;
  loaded_bitmap Cape;
  loaded_bitmap Torso;
};

struct low_entity {
  sim_entity Sim;
  world_position P;
};

struct entity_visible_piece {
  loaded_bitmap *Bitmap;
  v2 Offset;
  real32 OffsetZ;
  real32 EntityZC;
  real32 R, G, B, A;
  v2 Dim;
};

struct game_state {
  memory_arena WorldArena;
  world *World;

  // TODO: Should we allow split-screen
  uint32 CameraFollowingEntityIndex;
  world_position CameraP;

  uint32 PlayerIndexForController[ArrayCount(((game_input *)0)->Controllers)];

  // TODO: change the name to stored_entity
  uint32 LowEntityCount;
  low_entity LowEntities[100000];


  loaded_bitmap Backdrop;
  loaded_bitmap Shadow;
  hero_bitmaps HeroBitmaps[4];

  loaded_bitmap Tree;
  loaded_bitmap Sword;

  real32 MetersToPixels;
};

// TODO: This is dumb, this should just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct entity_visible_piece_group {
  game_state *GameState;
  uint32 PieceCount;
  entity_visible_piece Pieces[32];
};

inline low_entity *GetLowEntity(game_state *GameState, uint32 LowIndex) {
  low_entity *Result = 0;

  if ((LowIndex > 0) && LowIndex < GameState->LowEntityCount) {
    Result = GameState->LowEntities + LowIndex;
  }

  return Result;
}

#define HANDMADE_H
#endif
