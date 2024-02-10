#if !defined(HANDMADE_TILE_H)

struct TileMapPosition {
  // NOTE: These are fixed point tile locations.
  // The high bits are the tile chunk index, and the
  // low bits are the tile index in the chunk.
  uint32 AbsTileX;
  uint32 AbsTileY;

  // NOTE: This is tile-relative X and Y in ...pixels?
  real32 TileRelX;
  real32 TileRelY;
};

struct TileChunkPosition {
  uint32 TileChunkX;
  uint32 TileChunkY;

  uint32 RelTileX;
  uint32 RelTileY;
};

struct TileChunk {
  uint32 *Tiles;
};

struct TileMap {
  uint32 ChunkShift;
  uint32 ChunkMask;
  uint32 ChunkDim;

  real32 TileSideInMeters;
  int32 TileSideInPixels;
  real32 MetersToPixels;

  uint32 TileChunkCountX;
  uint32 TileChunkCountY;

  TileChunk *TileChunks;
};


#define HANDMADE_TILE_H
#endif
