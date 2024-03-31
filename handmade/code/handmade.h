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

inline void InitializeArena(memory_arena *Arena, memory_index Size, void *Base) {
  Arena->Size = Size;
  Arena->Base = (uint8 *)Base;
  Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count) * sizeof(type))
inline void * PushSize_(memory_arena *Arena, memory_index Size) {
  Assert((Arena->Used + Size) <= Arena->Size);
  void *Result = Arena->Base + Arena->Used;
  Arena->Used += Size;

  return (Result);
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
inline void ZeroSize(memory_index Size, void *Ptr) {
  // TODO: Check this guy for performance
  uint8 *Byte = (uint8 *)Ptr;
  while(Size--) {
    *Byte++ = 0;
  }
}

#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_entity.h"

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
  // TODO: It`s kind of busted that P`s can be invalid here,
  // AND we store whether they would be invalid in the flags field...
  // Can we do something better here?
  world_position P;
  sim_entity Sim;
};

struct entity_visible_piece {
  loaded_bitmap *Bitmap;
  v2 Offset;
  real32 OffsetZ;
  real32 EntityZC;
  real32 R, G, B, A;
  v2 Dim;
};

struct controlled_hero {
  uint32 EntityIndex;

  // NOTE: These are the controller requests for the simulation
  v2 ddP;
  v2 dSword;
  real32 dZ;
};

struct game_state {
  memory_arena WorldArena;
  world *World;

  // TODO: Should we allow split-screen
  uint32 CameraFollowingEntityIndex;
  world_position CameraP;

  controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];

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
