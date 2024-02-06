#include "handmade.h"

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
/*
internal void RenderWeirdGradient(GameOffscreenBuffer *Buffer, int BlueOffset, int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; Y++) {
    uint32 *Pixel = (uint32 *)Row;

    for (int X = 0; X < Buffer->Width; X++) {
      uint8 Blue = (uint8)(X + BlueOffset);
      uint8 Green = (uint8)(Y + GreenOffset);

      *Pixel++ = ((Green << 16) | Blue);  // understand << (shift) operator
    }

    Row += Buffer->Pitch;
  }
}
*/

inline uint32 RoundReal32ToUInt32(real32 Real32) {
  uint32 Result = (uint32)(Real32 + 0.5f);
  return Result;
}

inline int32 RoundReal32ToInt32(real32 Real32) {
  int32 Result = (int32)(Real32 + 0.5f);
  return Result;
}

inline int32 TruncateReal32ToInt32(real32 Real32) {
  int32 Result = (int32)Real32;
  return Result;
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

inline uint32 GetTileValueUnchecked(TileMap *Map, int32 TileX, int32 TileY) {
  uint32 TileMapValue = Map->Tiles[TileY * Map->CountX + TileX];
  return TileMapValue;
}

internal bool32 IsTileMapPointEmpty(TileMap *Map, real32 TestX, real32 TestY) {
  bool32 Empty = false;
  int32 PlayerTileX = TruncateReal32ToInt32((TestX - Map->UpperLeftX) / Map->TileWidth);
  int32 PlayerTileY = TruncateReal32ToInt32((TestY - Map->UpperLeftY) / Map->TileHeight);

  if ((PlayerTileX >= 0) && (PlayerTileX < Map->CountX) && (PlayerTileY >= 0) && (PlayerTileY < Map->CountY)) {
    uint32 TileMapValue = GetTileValueUnchecked(Map, PlayerTileX, PlayerTileY);
    Empty = TileMapValue == 0;
  }

  return Empty;
}

internal bool32 IsWorldPointEmpty(WorldMap *World, int32 TileMapX, int32 TileMapY, real32 TestX, real32 TestY) {
  bool32 Empty = false;

  TileMap *Map = GetTileMap(World, TileMapX, TileMapY);

  if (Map) {
    int32 PlayerTileX = TruncateReal32ToInt32((TestX - Map->UpperLeftX) / Map->TileWidth);
    int32 PlayerTileY = TruncateReal32ToInt32((TestY - Map->UpperLeftY) / Map->TileHeight);

    if ((PlayerTileX >= 0) && (PlayerTileX < Map->CountX) && (PlayerTileY >= 0) && (PlayerTileY < Map->CountY)) {
      uint32 TileMapValue = GetTileValueUnchecked(Map, PlayerTileX, PlayerTileY);
      Empty = TileMapValue == 0;
    }
  }

  return Empty;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(GameState) <= Memory->PermanentStorageSize);

#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9

  uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  TileMap TileMaps[2][2];
  TileMaps[0][0].CountX = TILE_MAP_COUNT_X;
  TileMaps[0][0].CountY = TILE_MAP_COUNT_Y;
  TileMaps[0][0].UpperLeftX = -30;
  TileMaps[0][0].UpperLeftY = 0;
  TileMaps[0][0].TileWidth = 60;
  TileMaps[0][0].TileHeight = 60;
  TileMaps[0][0].Tiles = (uint32 *)Tiles00;

  TileMaps[0][1] = TileMaps[0][0];
  TileMaps[0][1].Tiles = (uint32 *)Tiles01;

  TileMaps[1][0] = TileMaps[0][0];
  TileMaps[1][0].Tiles = (uint32 *)Tiles10;

  TileMaps[1][1] = TileMaps[0][0];
  TileMaps[1][1].Tiles = (uint32 *)Tiles11;

  TileMap *Map = &TileMaps[0][0];

  WorldMap World;
  World.TileMapCountX = 2;
  World.TileMapCountY = 2;
  World.TileMaps = (TileMap *)TileMaps;

  real32 PlayerWidth = 0.75f * Map->TileWidth;
  real32 PlayerHeight = Map->TileHeight;

  GameState *State = (GameState *)Memory->PermanentStorage;

  if (!Memory->IsInitialized) {
    State->PlayerX = 150;
    State->PlayerY = 150;
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

      if (
          IsTileMapPointEmpty(Map, NewPlayerX - 0.5f * PlayerWidth, NewPlayerY) &&
          IsTileMapPointEmpty(Map, NewPlayerX + 0.5f * PlayerWidth, NewPlayerY) &&
          IsTileMapPointEmpty(Map, NewPlayerX, NewPlayerY)) {
        State->PlayerX = NewPlayerX;
        State->PlayerY = NewPlayerY;
      }
    }
  }

  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 0.1f, 0.5f, 0.1f);

  for (int Row = 0; Row < 9; ++Row) {
    for (int Column = 0; Column < 17; ++Column) {
      uint32 TileID = GetTileValueUnchecked(Map, Column, Row);
      real32 Gray = 0.5f;
      if (TileID == 1) {
        Gray = 1.0f;
      }

      real32 MinX = Map->UpperLeftX + ((real32)Column) * Map->TileWidth;
      real32 MinY = Map->UpperLeftY + ((real32)Row) * Map->TileHeight;
      real32 MaxX = MinX + Map->TileWidth;
      real32 MaxY = MinY + Map->TileHeight;
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
