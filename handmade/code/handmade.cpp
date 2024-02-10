#include "handmade.h"
#include "handmade_tile.cpp"

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

internal void InitializeArena(MemoryArena *Arena, memory_index Size, uint8 *Base) {
  Arena->Size = Size;
  Arena->Base = Base;
  Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count) * sizeof(type))
void * PushSize_(MemoryArena *Arena, memory_index Size) {
  Assert((Arena->Used + Size) <= Arena->Size);
  void *Result = Arena->Base + Arena->Used;
  Arena->Used += Size;
  return Result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(GameState) <= Memory->PermanentStorageSize);

  real32 PlayerHeight = 1.4f;
  real32 PlayerWidth = 0.75f * PlayerHeight;

  GameState *State = (GameState *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    State->PlayerP.AbsTileX = 1;
    State->PlayerP.AbsTileY = 3;
    State->PlayerP.TileRelX = 5.0f;
    State->PlayerP.TileRelY = 5.0f;
    InitializeArena(&State->WorldArena, Memory->PermanentStorageSize - sizeof(GameState), (uint8 *)Memory->PermanentStorage + sizeof(GameState));

    State->World = PushStruct(&State->WorldArena, WorldMap);
    WorldMap *World = State->World;
    World->Map = PushStruct(&State->WorldArena, TileMap);

    TileMap *Map = World->Map;
    // NOTE: This is set to using 256x256 tile chunks
    Map->ChunkShift = 4;
    Map->ChunkMask = (1 << Map->ChunkShift) - 1;
    Map->ChunkDim = (1 << Map->ChunkShift);
    Map->TileChunkCountX = 128;
    Map->TileChunkCountY = 128;
    Map->TileChunks = PushArray(&State->WorldArena, Map->TileChunkCountX * Map->TileChunkCountY, TileChunk);

    for(uint32 Y = 0; Y < Map->TileChunkCountY; ++Y) {
      for(uint32 X = 0; X < Map->TileChunkCountX; ++X) {
        Map->TileChunks[Y * Map->TileChunkCountX + X].Tiles = PushArray(&State->WorldArena, Map->ChunkDim * Map->ChunkDim, uint32);
      }
    }

    Map->TileSideInMeters = 1.4f;
    Map->TileSideInPixels = 60;
    Map->MetersToPixels = (real32)Map->TileSideInPixels / (real32)Map->TileSideInMeters;

    real32 LowerLeftX = -(real32)Map->TileSideInPixels / 2;
    real32 LowerLeftY = (real32)Buffer->Height;

    uint32 TilesPerWidth = 17;
    uint32 TilesPerHeight = 9;
    for (uint32 ScreenY = 0; ScreenY < 32; ++ScreenY) {
      for (uint32 ScreenX = 0; ScreenX < 32; ++ScreenX) {
        for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY) {
          for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX) {
            uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
            uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;
            SetTileValue(&State->WorldArena, World->Map, AbsTileX, AbsTileY, ((TileX == TileY) && (TileY % 2)) ? 1 : 0);
          }
        }
      }
    }

    Memory->IsInitialized = true;
  }

  WorldMap *World = State->World;
  TileMap *Map = World->Map;


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

      real32 PlayerSpeed = 2.0f;
      if (Controller->ActionUp.EndedDown) {
        PlayerSpeed = 7.0f;
      }

      dPlayerX *= PlayerSpeed;
      dPlayerY *= PlayerSpeed;

      TileMapPosition NewPlayerP = State->PlayerP;
      NewPlayerP.TileRelX += Input->dtForFrame * dPlayerX;
      NewPlayerP.TileRelY += Input->dtForFrame * dPlayerY;
      NewPlayerP = RecanonicalizePosition(Map, NewPlayerP);

      TileMapPosition PlayerLeft = NewPlayerP;
      PlayerLeft.TileRelX -= 0.5f * PlayerWidth;
      PlayerLeft = RecanonicalizePosition(Map, PlayerLeft);

      TileMapPosition PlayerRight = NewPlayerP;
      PlayerRight.TileRelX += 0.5f * PlayerWidth;
      PlayerRight = RecanonicalizePosition(Map, PlayerRight);

      if (IsTileMapPointEmpty(Map, NewPlayerP) && IsTileMapPointEmpty(Map, PlayerLeft) && IsTileMapPointEmpty(Map, PlayerRight)) {
        State->PlayerP = NewPlayerP;
      }
    }
  }

  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 0.1f, 0.5f, 0.1f);

  real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;
  real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;

  for (int32 RelRow = -10; RelRow < 10; ++RelRow) {
    for (int32 RelColumn = -20; RelColumn < 20; ++RelColumn) {
      uint32 Column = State->PlayerP.AbsTileX + RelColumn;
      uint32 Row = State->PlayerP.AbsTileY + RelRow;

      uint32 TileID = GetTileValue(Map, Column, Row);
      real32 Gray = 0.5f;
      if (TileID == 1) {
        Gray = 1.0f;
      }

      if ((Column == State->PlayerP.AbsTileX) && (Row == State->PlayerP.AbsTileY)) {
        Gray = 0.0f;
      }

      real32 CenterX = ScreenCenterX - Map->MetersToPixels * State->PlayerP.TileRelX + ((real32)RelColumn) * Map->TileSideInPixels;
      real32 CenterY = ScreenCenterY + Map->MetersToPixels * State->PlayerP.TileRelY - ((real32)RelRow) * Map->TileSideInPixels;

      real32 MinX = CenterX - 0.5f * Map->TileSideInPixels;
      real32 MinY = CenterY - 0.5f * Map->TileSideInPixels;

      real32 MaxX = CenterX + 0.5f * Map->TileSideInPixels;
      real32 MaxY = CenterY + 0.5f * Map->TileSideInPixels;
      DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
    }
  }

  real32 PlayerR = 1.0f;
  real32 PlayerG = 1.0f;
  real32 PlayerB = 0.0f;
  real32 PlayerLeft = ScreenCenterX - 0.5f * Map->MetersToPixels * PlayerWidth;
  real32 PlayerTop = ScreenCenterY - Map->MetersToPixels * PlayerHeight;

  DrawRectangle(Buffer, PlayerLeft, PlayerTop, PlayerLeft + Map->MetersToPixels * PlayerWidth, PlayerTop + Map->MetersToPixels * PlayerHeight, PlayerR, PlayerG, PlayerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  GameState *State = (GameState *)Memory->PermanentStorage;
  GameOutputSound(State, SoundBuffer, 400);
}
