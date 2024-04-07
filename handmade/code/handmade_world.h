#if !defined(HANDMADE_WORLD_H)

struct world_position {
  // TODO: Puzzler! How can we get rid of AbsTile* here,
  // and still allow references to entities to be able to figure
  // out where they are (or rather, which world_chunk they are in?)

  int32 ChunkX;
  int32 ChunkY;
  int32 ChunkZ;

  // NOTE: This is the offset from the chunk center
  v3 Offset;
};

// TODO: Could make this just tile_chunk and then allow multiple tile chunks per X/Y/Z
struct world_entity_block {
  uint32 EntityCount;
  uint32 LowEntityIndex[16];
  world_entity_block *Next;
};

struct world_chunk {
  int32 ChunkX;
  int32 ChunkY;
  int32 ChunkZ;

  // TODO: Profile this and determine if a pointer would be better here!
  world_entity_block FirstBlock;
  world_chunk *NextInHash;
};

struct world {
  real32 TileSideInMeters;
  real32 TileDepthInMeters;
  v3 ChunkDimInMeters;

  world_entity_block *FirstFree;

  // TODO: WorldChunkHash should probably switch to pointers IF
  // tile entity blocks continue to be stored en masse directly in the tile chunk!
  // NOTE: At the moment, this must be a power of two!
  world_chunk ChunkHash[4096];
};

#define HANDMADE_WORLD_H
#endif
