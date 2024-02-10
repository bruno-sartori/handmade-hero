
inline void RecanonicalizeCoord(TileMap *Map, uint32 *Tile, real32 *TileRel) {
  // NOTE: World is assumed to be toroidal topology, if you step off one end you come back on the other!

  int32 Offset = RoundReal32ToInt32(*TileRel / Map->TileSideInMeters);

  *Tile += Offset;
  *TileRel -= Offset * Map->TileSideInMeters;

  Assert(*TileRel >= -0.5f * Map->TileSideInMeters);
  Assert(*TileRel <= 0.5f * Map->TileSideInMeters);
}

inline TileMapPosition RecanonicalizePosition(TileMap *Map, TileMapPosition Position) {
  TileMapPosition Result = Position;

  RecanonicalizeCoord(Map, &Result.AbsTileX, &Result.TileRelX);
  RecanonicalizeCoord(Map, &Result.AbsTileY, &Result.TileRelY);

  return Result;
}

inline TileChunk* GetTileChunk(TileMap *Map, uint32 TileChunkX, uint32 TileChunkY) {
  TileChunk *Chunk = 0;
  if ((TileChunkX >= 0) && (TileChunkX < Map->TileChunkCountX) && (TileChunkY >= 0) && (TileChunkY < Map->TileChunkCountY)) {
    Chunk = &Map->TileChunks[TileChunkY * Map->TileChunkCountX + TileChunkX];
  }
  return Chunk;
}

inline uint32 GetTileValueUnchecked(TileMap *Map, TileChunk *Chunk, uint32 TileX, uint32 TileY) {
  Assert(Chunk);
  Assert(TileX < Map->ChunkDim);
  Assert(TileY < Map->ChunkDim);

  uint32 TileChunkValue = Chunk->Tiles[TileY * Map->ChunkDim + TileX];
  return TileChunkValue;
}

inline void SetTileValueUnchecked(TileMap *Map, TileChunk *Chunk, uint32 TileX, uint32 TileY, uint32 TileValue) {
  Assert(Chunk);
  Assert(TileX < Map->ChunkDim);
  Assert(TileY < Map->ChunkDim);

  Chunk->Tiles[TileY * Map->ChunkDim + TileX] = TileValue;
}

inline uint32 GetTileChunkValue(TileMap *Map, TileChunk *Chunk, uint32 TestTileX, uint32 TestTileY) {
  uint32 TileChunkValue = 0;

  if (Chunk) {
    TileChunkValue = GetTileValueUnchecked(Map, Chunk, TestTileX, TestTileY);
  }

  return TileChunkValue;
}

inline void SetTileChunkValue(TileMap *Map, TileChunk *Chunk, uint32 TestTileX, uint32 TestTileY, uint32 TileValue) {
  uint32 TileChunkValue = 0;

  if (Chunk) {
    SetTileValueUnchecked(Map, Chunk, TestTileX, TestTileY, TileValue);
  }
}

internal TileChunkPosition GetChunkPositionFor(TileMap *Map, uint32 AbsTileX, uint32 AbsTileY) {
  TileChunkPosition Result;
  Result.TileChunkX = AbsTileX >> Map->ChunkShift;
  Result.TileChunkY = AbsTileY >> Map->ChunkShift;
  Result.RelTileX = AbsTileX & Map->ChunkMask;
  Result.RelTileY = AbsTileY & Map->ChunkMask;

  return Result;
}

internal uint32 GetTileValue(TileMap *Map, uint32 AbsTileX, uint32 AbsTileY) {
  TileChunkPosition ChunkPos = GetChunkPositionFor(Map, AbsTileX, AbsTileY);
  TileChunk *Chunk = GetTileChunk(Map, ChunkPos.TileChunkX, ChunkPos.TileChunkX);
  uint32 TileChunkValue = GetTileChunkValue(Map, Chunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
  return TileChunkValue;
}

internal bool32 IsTileMapPointEmpty(TileMap *Map, TileMapPosition Position) {
  uint32 TileChunkValue = GetTileValue(Map, Position.AbsTileX, Position.AbsTileY);
  bool32 Empty = (TileChunkValue == 0);
  return Empty;
}

internal void SetTileValue(MemoryArena *Arena, TileMap *Map, uint32 AbsTileX, uint32 AbsTileY, uint32 TileValue) {
  TileChunkPosition ChunkPos = GetChunkPositionFor(Map, AbsTileX, AbsTileY);
  TileChunk *Chunk = GetTileChunk(Map, ChunkPos.TileChunkX, ChunkPos.TileChunkX);
  Assert(Chunk);

  SetTileChunkValue(Map, Chunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}
