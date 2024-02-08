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

inline CanonicalPosition GetCanonicalPosition(WorldMap *World, RawPosition Position) {
  CanonicalPosition Result;

  Result.TileMapX = Position.TileMapX;
  Result.TileMapY = Position.TileMapY;

  // NOTE: Gets the tile map position relative to player`s position in tiles
  real32 X = Position.X - World->UpperLeftX;
  real32 Y = Position.Y - World->UpperLeftY;
  Result.TileX = FloorReal32ToInt32(X / World->TileSideInPixels);
  Result.TileY = FloorReal32ToInt32(Y / World->TileSideInPixels);

  // NOTE: Gets the player`s position relative to the tile he is in
  Result.TileRelX = X - Result.TileX * World->TileSideInPixels;
  Result.TileRelY = Y - Result.TileY * World->TileSideInPixels;

  Assert(Result.TileRelX >= 0);
  Assert(Result.TileRelY >= 0);
  Assert(Result.TileRelX < World->TileSideInPixels);
  Assert(Result.TileRelY < World->TileSideInPixels);


  if (Result.TileX < 0) {
    Result.TileX = World->CountX + Result.TileX;
    --Result.TileMapX;
  }

  if (Result.TileY < 0) {
    Result.TileY = World->CountY + Result.TileY;
    --Result.TileMapY;
  }

  if (Result.TileX >= World->CountX) {
    Result.TileX = Result.TileX - World->CountX;
    ++Result.TileMapX;
  }

  if (Result.TileY >= World->CountY) {
    Result.TileY = Result.TileY - World->CountY;
    ++Result.TileMapY;
  }

  return Result;
}

internal bool32 IsWorldPointEmpty(WorldMap *World, RawPosition TestPosition) {
  bool32 Empty = false;

  CanonicalPosition CanonicalPos = GetCanonicalPosition(World, TestPosition);

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
  World.UpperLeftX = -(real32)World.TileSideInPixels / 2;
  World.UpperLeftY = 0;

  real32 PlayerWidth = 0.75f * World.TileSideInPixels;
  real32 PlayerHeight = (real32)World.TileSideInPixels;

  World.TileMaps = (TileMap *)TileMaps;

  GameState *State = (GameState *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    State->PlayerX = 150;
    State->PlayerY = 150;
    Memory->IsInitialized = true;
  }

  TileMap *Map = GetTileMap(&World, State->PlayerTileMapX, State->PlayerTileMapY);
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

      dPlayerX *= 64.0f;
      dPlayerY *= 64.0f;

      real32 NewPlayerX = State->PlayerX + Input->dtForFrame * dPlayerX;
      real32 NewPlayerY = State->PlayerY + Input->dtForFrame * dPlayerY;

      RawPosition PlayerPos = {
        State->PlayerTileMapX,
        State->PlayerTileMapY,
        NewPlayerX,
        NewPlayerY,
      };
      RawPosition PlayerLeft = PlayerPos;
      PlayerLeft.X -= 0.5f * PlayerWidth;
      RawPosition PlayerRight = PlayerPos;
      PlayerRight.X += 0.5f * PlayerWidth;

      if (IsWorldPointEmpty(&World, PlayerPos) && IsWorldPointEmpty(&World, PlayerLeft) && IsWorldPointEmpty(&World, PlayerRight)) {
        CanonicalPosition CanonicalPos = GetCanonicalPosition(&World, PlayerPos);
        State->PlayerTileMapX = CanonicalPos.TileMapX;
        State->PlayerTileMapY = CanonicalPos.TileMapY;

        State->PlayerX = World.UpperLeftX + World.TileSideInPixels * CanonicalPos.TileX + CanonicalPos.TileRelX;
        State->PlayerY = World.UpperLeftY + World.TileSideInPixels * CanonicalPos.TileY + CanonicalPos.TileRelY;
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
  real32 PlayerLeft = State->PlayerX - 0.5f * PlayerWidth;
  real32 PlayerTop = State->PlayerY - PlayerHeight;

  DrawRectangle(Buffer, PlayerLeft, PlayerTop, PlayerLeft + PlayerWidth, PlayerTop + PlayerHeight, PlayerR, PlayerG, PlayerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  GameState *State = (GameState *)Memory->PermanentStorage;
  GameOutputSound(State, SoundBuffer, 400);
}
