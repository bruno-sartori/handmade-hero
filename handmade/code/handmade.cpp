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

inline TileMap* GetTileMap(WorldMap *World, int32 TileMapX, int32 TileMapY) {
  TileMap *TileMap = 0;
  if ((TileMapX >= 0) && (TileMapX < World->TileMapCountX) && (TileMapY >= 0) && (TileMapY < World->TileMapCountY)) {
    TileMap = &World->TileMaps[TileMapY * World->TileMapCountX + TileMapX];
  }
  return TileMap;
}

inline uint32 GetTileValueUnchecked(WorldMap *World, TileMap *Map, int32 TileX, int32 TileY) {
  Assert(Map);
  Assert((TileX >= 0) && (TileX < World->CountX) && (TileY >= 0) && (TileY < World->CountY));

  uint32 TileMapValue = Map->Tiles[TileY * World->CountX + TileX];
  return TileMapValue;
}

inline bool32 IsTileMapPointEmpty(WorldMap *World, TileMap *Map, int32 TestTileX, int32 TestTileY) {
  bool32 Empty = false;

  if (Map) {
    if ((TestTileX >= 0) && (TestTileX < World->CountX) && (TestTileY >= 0) && (TestTileY < World->CountY)) {
      uint32 TileMapValue = GetTileValueUnchecked(World, Map, TestTileX, TestTileY);
      Empty = TileMapValue == 0;
    }
  }

  return Empty;
}

inline void RecanonicalizeCoord(WorldMap *World, int32 TileCount, int32 *TileMap, int32 *Tile, real32 *TileRel) {
  int32 Offset = FloorReal32ToInt32(*TileRel / World->TileSideInMeters);
  *Tile += Offset;
  *TileRel -= Offset * World->TileSideInMeters;

  Assert(*TileRel >= 0);
  Assert(*TileRel <= World->TileSideInMeters);

  if (*Tile < 0) {
    *Tile = TileCount + *Tile;
    --*TileMap;
  }

  if (*Tile >= TileCount) {
    *Tile = *Tile - TileCount;
    ++*TileMap;
  }
}

inline CanonicalPosition RecanonicalizePosition(WorldMap *World, CanonicalPosition Position) {
  CanonicalPosition Result = Position;

  RecanonicalizeCoord(World, World->CountX, &Result.TileMapX, &Result.TileX, &Result.TileRelX);
  RecanonicalizeCoord(World, World->CountY, &Result.TileMapY, &Result.TileY, &Result.TileRelY);

  return Result;
}

internal bool32 IsWorldPointEmpty(WorldMap *World, CanonicalPosition CanonicalPos) {
  bool32 Empty = false;

  TileMap *Map = GetTileMap(World, CanonicalPos.TileMapX, CanonicalPos.TileMapY);
  Empty = IsTileMapPointEmpty(World, Map, CanonicalPos.TileX, CanonicalPos.TileY);

  return Empty;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(GameState) <= Memory->PermanentStorageSize);

#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9

/*
       TILE MAPS POSITION
    ------------------------
    |          |           |
    |   00     |    10     |
    |          |           |
    |----------|-----------|
    |          |           |
    |   01     |    11     |
    |          |           |
    ------------------------
*/

  uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
      {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
      {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
      {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
  };

  uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
      {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 1, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 1, 0,  1, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
  };

  uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {0, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
  };

  uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0, 1},
      {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 1, 0,  0, 0, 0, 0, 1},
      {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 1, 0, 0, 1},
      {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
      {1, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
      {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
  };

  TileMap TileMaps[2][2];

  TileMaps[0][0].Tiles = (uint32 *)Tiles00;
  TileMaps[0][1].Tiles = (uint32 *)Tiles10;
  TileMaps[1][0].Tiles = (uint32 *)Tiles01;
  TileMaps[1][1].Tiles = (uint32 *)Tiles11;

  WorldMap World;
  World.TileMapCountX = 2;
  World.TileMapCountY = 2;
  World.CountX = TILE_MAP_COUNT_X;
  World.CountY = TILE_MAP_COUNT_Y;
  World.TileSideInMeters = 1.4f;
  World.TileSideInPixels = 60;
  World.MetersToPixels = (real32)World.TileSideInPixels / (real32)World.TileSideInMeters;
  World.UpperLeftX = -(real32)World.TileSideInPixels / 2;
  World.UpperLeftY = 0;

  real32 PlayerHeight = 1.4f;
  real32 PlayerWidth = 0.75f * PlayerHeight;

  World.TileMaps = (TileMap *)TileMaps;

  GameState *State = (GameState *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    State->PlayerP.TileMapX = 0;
    State->PlayerP.TileMapY = 0;
    State->PlayerP.TileX = 3;
    State->PlayerP.TileY = 3;
    State->PlayerP.TileRelX = 5.0f;
    State->PlayerP.TileRelY = 5.0f;

    Memory->IsInitialized = true;
  }

  TileMap *Map = GetTileMap(&World, State->PlayerP.TileMapX, State->PlayerP.TileMapY);
  Assert(Map);

  for (int ControllerIdx = 0; ControllerIdx < ArrayCount(Input->Controllers); ++ControllerIdx) {
    GameControllerInput *Controller = GetController(Input, ControllerIdx);

    if (Controller->IsAnalog) {
      // NOTE: Use analog movement tuning
    } else {
      // NOTE: Use digital movement tuning
      real32 dPlayerX = 0.0f;  // pixels/second
      real32 dPlayerY = 0.0f;
      if (Controller->MoveUp.EndedDown) {
        dPlayerY = -1.0f;
      }
      if (Controller->MoveDown.EndedDown) {
        dPlayerY = 1.0f;
      }
      if (Controller->MoveLeft.EndedDown) {
        dPlayerX = -1.0f;
      }
      if (Controller->MoveRight.EndedDown) {
        dPlayerX = 1.0f;
      }

      dPlayerX *= 2.0f;
      dPlayerY *= 2.0f;

      CanonicalPosition NewPlayerP = State->PlayerP;
      NewPlayerP.TileRelX += Input->dtForFrame * dPlayerX;
      NewPlayerP.TileRelY += Input->dtForFrame * dPlayerY;
      NewPlayerP = RecanonicalizePosition(&World, NewPlayerP);

      CanonicalPosition PlayerLeft = NewPlayerP;
      PlayerLeft.TileRelX -= 0.5f * PlayerWidth;
      PlayerLeft = RecanonicalizePosition(&World, PlayerLeft);

      CanonicalPosition PlayerRight = NewPlayerP;
      PlayerRight.TileRelX += 0.5f * PlayerWidth;
      PlayerRight = RecanonicalizePosition(&World, PlayerRight);

      if (IsWorldPointEmpty(&World, NewPlayerP) && IsWorldPointEmpty(&World, PlayerLeft) && IsWorldPointEmpty(&World, PlayerRight)) {
        State->PlayerP = NewPlayerP;
      }
    }
  }

  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 0.1f, 0.5f, 0.1f);

  for (int Row = 0; Row < 9; ++Row) {
    for (int Column = 0; Column < 17; ++Column) {
      uint32 TileID = GetTileValueUnchecked(&World, Map, Column, Row);
      real32 Gray = 0.5f;
      if (TileID == 1) {
        Gray = 1.0f;
      }

      if ((Row == State->PlayerP.TileY) && (Column == State->PlayerP.TileX)) {
        Gray = 0.0f;
      }

      real32 MinX = World.UpperLeftX + ((real32)Column) * World.TileSideInPixels;
      real32 MinY = World.UpperLeftY + ((real32)Row) * World.TileSideInPixels;
      real32 MaxX = MinX + World.TileSideInPixels;
      real32 MaxY = MinY + World.TileSideInPixels;
      DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
    }
  }

  real32 PlayerR = 1.0f;
  real32 PlayerG = 1.0f;
  real32 PlayerB = 0.0f;
  real32 PlayerLeft = World.UpperLeftX + World.TileSideInPixels * State->PlayerP.TileX + World.MetersToPixels * State->PlayerP.TileRelX - 0.5f * World.MetersToPixels * PlayerWidth;
  real32 PlayerTop = World.UpperLeftY + World.TileSideInPixels * State->PlayerP.TileY + World.MetersToPixels * State->PlayerP.TileRelY - World.MetersToPixels * PlayerHeight;

  DrawRectangle(Buffer, PlayerLeft, PlayerTop, PlayerLeft + World.MetersToPixels * PlayerWidth, PlayerTop + World.MetersToPixels * PlayerHeight, PlayerR, PlayerG, PlayerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  GameState *State = (GameState *)Memory->PermanentStorage;
  GameOutputSound(State, SoundBuffer, 400);
}
