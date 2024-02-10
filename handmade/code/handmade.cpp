#include "handmade.h"
#include "handmade_intrinsics.h"

internal void GameOutputSound(GameState *State, GameSoundOutputBuffer *SoundBuffer, int ToneHz) {
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
  int16 *SampleOut = SoundBuffer->Samples;

  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
#if 0
    real32 SineValue = sinf(State->tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
    int16 SampleValue = 0;
#endif
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;
#if 0
    State->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;

    if (State->tSine > 2.0f * Pi32) {
      State->tSine -= 2.0f * Pi32;
    }
#endif
  }
}

internal void DrawRectangle(GameOffscreenBuffer *Buffer, real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY, real32 R, real32 G, real32 B) {
  int32 MinX = RoundReal32ToInt32(RealMinX);
  int32 MinY = RoundReal32ToInt32(RealMinY);
  int32 MaxX = RoundReal32ToInt32(RealMaxX);
  int32 MaxY = RoundReal32ToInt32(RealMaxY);

  if (MinX < 0) {
    MinX = 0;
  }

  if (MinY < 0) {
    MinY = 0;
  }

  if (MaxX > Buffer->Width) {
    MaxX = Buffer->Width;
  }

  if (MaxY > Buffer->Height) {
    MaxY = Buffer->Height;
  }

  // NOTE: Bit Pattern: 0X AA RR GG BB
  uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) | (RoundReal32ToUInt32(G * 255.0f) << 8) | (RoundReal32ToUInt32(B * 255.0f) << 0));

  uint8 *Row = ((uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel + MinY * Buffer->Pitch);

  for (int Y = MinY; Y < MaxY; ++Y) {
    uint32 *Pixel = (uint32 *)Row;
    for (int X = MinX; X < MaxX; ++X) {
      *Pixel++ = Color;
    }
    Row += Buffer->Pitch;
  }
}

inline TileChunk* GetTileChunk(WorldMap *World, int32 TileChunkX, int32 TileChunkY) {
  TileChunk *Chunk = 0;
  if ((TileChunkX >= 0) && (TileChunkX < World->TileChunkCountX) && (TileChunkY >= 0) && (TileChunkY < World->TileChunkCountY)) {
    Chunk = &World->TileChunks[TileChunkY * World->TileChunkCountX + TileChunkX];
  }
  return Chunk;
}

inline uint32 GetTileValueUnchecked(WorldMap *World, TileChunk *Chunk, uint32 TileX, uint32 TileY) {
  Assert(Chunk);
  Assert(TileX < World->ChunkDim);
  Assert(TileY < World->ChunkDim);

  uint32 TileChunkValue = Chunk->Tiles[TileY * World->ChunkDim + TileX];
  return TileChunkValue;
}

inline uint32 GetTileChunkValue(WorldMap *World, TileChunk *Chunk, uint32 TestTileX, uint32 TestTileY) {
  uint32 TileChunkValue = 0;

  if (Chunk) {
    TileChunkValue = GetTileValueUnchecked(World, Chunk, TestTileX, TestTileY);
  }

  return TileChunkValue;
}

inline void RecanonicalizeCoord(WorldMap *World, uint32 *Tile, real32 *TileRel) {
  // NOTE: World is assumed to be toroidal topology, if you step off one end you come back on the other!

  int32 Offset = FloorReal32ToInt32(*TileRel / World->TileSideInMeters);

  *Tile += Offset;
  *TileRel -= Offset * World->TileSideInMeters;

  Assert(*TileRel >= 0);
  Assert(*TileRel <= World->TileSideInMeters);
}

inline WorldPosition RecanonicalizePosition(WorldMap *World, WorldPosition Position) {
  WorldPosition Result = Position;

  RecanonicalizeCoord(World, &Result.AbsTileX, &Result.TileRelX);
  RecanonicalizeCoord(World, &Result.AbsTileY, &Result.TileRelY);

  return Result;
}

internal TileChunkPosition GetChunkPositionFor(WorldMap *World, uint32 AbsTileX, uint32 AbsTileY) {
  TileChunkPosition Result;
  Result.TileChunkX = AbsTileX >> World->ChunkShift;
  Result.TileChunkY = AbsTileY >> World->ChunkShift;
  Result.RelTileX = AbsTileX & World->ChunkMask;
  Result.RelTileY = AbsTileY & World->ChunkMask;

  return Result;
}

internal uint32 GetTileValue(WorldMap *World, uint32 AbsTileX, uint32 AbsTileY) {
  TileChunkPosition ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY);
  TileChunk *Chunk = GetTileChunk(World, ChunkPos.TileChunkX, ChunkPos.TileChunkX);
  uint32 TileChunkValue = GetTileChunkValue(World, Chunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
  return TileChunkValue;
}

internal bool32 IsWorldPointEmpty(WorldMap *World, WorldPosition Position) {
  uint32 TileChunkValue = GetTileValue(World, Position.AbsTileX, Position.AbsTileY);
  bool32 Empty = (TileChunkValue == 0);
  return Empty;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(GameState) <= Memory->PermanentStorageSize);

#define TILE_MAP_COUNT_X 256
#define TILE_MAP_COUNT_Y 256

  uint32 TempTiles[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1, 1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1, 1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1, 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1, 1, 1, 0, 0,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1, 1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1, 1, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, 1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, 1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 1, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 1, 0,  0, 0, 0, 0, 1, 1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 1, 0, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 1, 0,  1, 0, 0, 0, 1, 1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, 1, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
  };

  WorldMap World;

  // NOTE: This is set to using 256x256 tile chunks
  World.ChunkShift = 8;
  World.ChunkMask = (1 << World.ChunkShift) - 1;
  World.ChunkDim = 256;

  World.TileChunkCountX = 1;
  World.TileChunkCountY = 1;

  TileChunk TilesChunk;
  TilesChunk.Tiles = (uint32 *)TempTiles;
  World.TileChunks = &TilesChunk;

  World.TileSideInMeters = 1.4f;
  World.TileSideInPixels = 60;
  World.MetersToPixels = (real32)World.TileSideInPixels / (real32)World.TileSideInMeters;

  real32 LowerLeftX = -(real32)World.TileSideInPixels / 2;
  real32 LowerLeftY = (real32)Buffer->Height;

  real32 PlayerHeight = 1.4f;
  real32 PlayerWidth = 0.75f * PlayerHeight;

  GameState *State = (GameState *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    State->PlayerP.AbsTileX = 3;
    State->PlayerP.AbsTileY = 3;
    State->PlayerP.TileRelX = 5.0f;
    State->PlayerP.TileRelY = 5.0f;

    Memory->IsInitialized = true;
  }

  for (int ControllerIdx = 0; ControllerIdx < ArrayCount(Input->Controllers); ++ControllerIdx) {
    GameControllerInput *Controller = GetController(Input, ControllerIdx);

    if (Controller->IsAnalog) {
      // NOTE: Use analog movement tuning
    } else {
      // NOTE: Use digital movement tuning
      real32 dPlayerX = 0.0f;  // pixels/second
      real32 dPlayerY = 0.0f;
      if (Controller->MoveUp.EndedDown) {
        dPlayerY = 1.0f;
      }
      if (Controller->MoveDown.EndedDown) {
        dPlayerY = -1.0f;
      }
      if (Controller->MoveLeft.EndedDown) {
        dPlayerX = -1.0f;
      }
      if (Controller->MoveRight.EndedDown) {
        dPlayerX = 1.0f;
      }

      dPlayerX *= 2.0f;
      dPlayerY *= 2.0f;

      WorldPosition NewPlayerP = State->PlayerP;
      NewPlayerP.TileRelX += Input->dtForFrame * dPlayerX;
      NewPlayerP.TileRelY += Input->dtForFrame * dPlayerY;
      NewPlayerP = RecanonicalizePosition(&World, NewPlayerP);

      WorldPosition PlayerLeft = NewPlayerP;
      PlayerLeft.TileRelX -= 0.5f * PlayerWidth;
      PlayerLeft = RecanonicalizePosition(&World, PlayerLeft);

      WorldPosition PlayerRight = NewPlayerP;
      PlayerRight.TileRelX += 0.5f * PlayerWidth;
      PlayerRight = RecanonicalizePosition(&World, PlayerRight);

      if (IsWorldPointEmpty(&World, NewPlayerP) && IsWorldPointEmpty(&World, PlayerLeft) && IsWorldPointEmpty(&World, PlayerRight)) {
        State->PlayerP = NewPlayerP;
      }
    }
  }

  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 0.1f, 0.5f, 0.1f);

  real32 CenterX = 0.5f * (real32)Buffer->Width;
  real32 CenterY = 0.5f * (real32)Buffer->Height;

  for (int32 RelRow = -10; RelRow < 10; ++RelRow) {
    for (int32 RelColumn = -20; RelColumn < 20; ++RelColumn) {
      uint32 Column = State->PlayerP.AbsTileX + RelColumn;
      uint32 Row = State->PlayerP.AbsTileY + RelRow;

      uint32 TileID = GetTileValue(&World, Column, Row);
      real32 Gray = 0.5f;
      if (TileID == 1) {
        Gray = 1.0f;
      }

      if ((Column == State->PlayerP.AbsTileX) && (Row == State->PlayerP.AbsTileY)) {
        Gray = 0.0f;
      }

      real32 MinX = CenterX + ((real32)RelColumn) * World.TileSideInPixels;
      real32 MinY = CenterY - ((real32)RelRow) * World.TileSideInPixels;
      real32 MaxX = MinX + World.TileSideInPixels;
      real32 MaxY = MinY - World.TileSideInPixels;
      DrawRectangle(Buffer, MinX, MaxY, MaxX, MinY, Gray, Gray, Gray);
    }
  }

  real32 PlayerR = 1.0f;
  real32 PlayerG = 1.0f;
  real32 PlayerB = 0.0f;
  real32 PlayerLeft = CenterX + World.MetersToPixels * State->PlayerP.TileRelX - 0.5f * World.MetersToPixels * PlayerWidth;
  real32 PlayerTop = CenterY - World.MetersToPixels * State->PlayerP.TileRelY - World.MetersToPixels * PlayerHeight;

  DrawRectangle(Buffer, PlayerLeft, PlayerTop, PlayerLeft + World.MetersToPixels * PlayerWidth, PlayerTop + World.MetersToPixels * PlayerHeight, PlayerR, PlayerG, PlayerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  GameState *State = (GameState *)Memory->PermanentStorage;
  GameOutputSound(State, SoundBuffer, 400);
}
