#include "handmade.h"
#include "handmade_random.h"
#include "handmade_tile.cpp"

internal void GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz) {
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
    // TODO: Draw this out for people
#if 0
    real32 SineValue = sinf(GameState->tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
    int16 SampleValue = 0;
#endif
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

#if 0
    GameState->tSine += 2.0f*Pi32*1.0f/(real32)WavePeriod;
    if(GameState->tSine > 2.0f*Pi32) {
      GameState->tSine -= 2.0f*Pi32;
    }
#endif
  }
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B) {
  int32 MinX = RoundReal32ToInt32(vMin.X);
  int32 MinY = RoundReal32ToInt32(vMin.Y);
  int32 MaxX = RoundReal32ToInt32(vMax.X);
  int32 MaxY = RoundReal32ToInt32(vMax.Y);

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

internal void DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY, int32 AlignX = 0, int32 AlignY = 0) {
#if 1
  RealX -= (real32)AlignX;
  RealY -= (real32)AlignY;

  int32 MinX = RoundReal32ToInt32(RealX);
  int32 MinY = RoundReal32ToInt32(RealY);
  int32 MaxX = RoundReal32ToInt32(RealX + (real32)Bitmap->Width);
  int32 MaxY = RoundReal32ToInt32(RealY + (real32)Bitmap->Height);

  int32 SourceOffsetX = 0;
  if (MinX < 0) {
    SourceOffsetX = -MinX;
    MinX = 0;
  }

  int32 SourceOffsetY = 0;
  if (MinY < 0) {
    SourceOffsetY = -MinY;
    MinY = 0;
  }

  if (MaxX > Buffer->Width) {
    MaxX = Buffer->Width;
  }

  if (MaxY > Buffer->Height) {
    MaxY = Buffer->Height;
  }

  // TODO: SourceRow needs to be changed based on clipping.
  uint32 *SourceRow = Bitmap->Pixels + Bitmap->Width * (Bitmap->Height - 1);
  SourceRow += -SourceOffsetY * Bitmap->Width + SourceOffsetX;
  uint8 *DestRow = ((uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel + MinY * Buffer->Pitch);

  for (int Y = MinY; Y < MaxY; ++Y) {
    uint32 *Dest = (uint32 *)DestRow;
    uint32 *Source = SourceRow;
    for (int X = MinX; X < MaxX; ++X) {
      real32 A = (real32)((*Source >> 24) & 0xFF) / 255.0f;

      real32 SR = (real32)((*Source >> 16) & 0xFF);
      real32 SG = (real32)((*Source >> 8) & 0xFF);
      real32 SB = (real32)((*Source >> 0) & 0xFF);

      real32 DR = (real32)((*Dest >> 16) & 0xFF);
      real32 DG = (real32)((*Dest >> 8) & 0xFF);
      real32 DB = (real32)((*Dest >> 0) & 0xFF);

      // TODO: someday, we need to talk about premultiplied alpha!
      // this is not premultiplied alpha!
      real32 R = (1.0f-A)*DR + A*SR;
      real32 G = (1.0f-A)*DG + A*SG;
      real32 B = (1.0f-A)*DB + A*SB;

      *Dest = (((uint32)(R + 0.5f) << 16) | ((uint32)(G + 0.5f) << 8) | ((uint32)(B + 0.5f) << 0));

      ++Dest;
      ++Source;
    }

    DestRow += Buffer->Pitch;
    SourceRow -= Bitmap->Width;
  }
#else
  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 0.1f);
#endif
}

// NOTE: Pragma pack(operation, byte) tells the compiler to align struct properties in desired bytes so that it does not
// changes the bytes size structure when dealing with different size properties and then we can use the struct to properly
// cast the file contents to it
#pragma pack(push, 1)
struct bitmap_header {
  uint16 FileType;
  uint32 FileSize;
  uint16 Reserved1;
  uint16 Reserved2;
  uint32 BitmapOffset;
  uint32 Size;
  int32 Width;
  int32 Height;
  uint16 Planes;
  uint16 BitsPerPixel;
  uint32 Compression;
  uint32 SizeOfBitmap;
  int32 HorzResolution;
  int32 VertResolution;
  uint32 ColorsUsed;
  uint32 ColorsImportant;

  uint32 RedMask;
  uint32 GreenMask;
  uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char* FileName) {
  loaded_bitmap Result = {};

  debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);
  if (ReadResult.ContentsSize != 0) {
    bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
    uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
    Result.Pixels = Pixels;
    Result.Width = Header->Width;
    Result.Height = Header->Height;

    Assert(Header->Compression == 3);

    // NOTE: If you are using this generically for some reason,
    // please remember that BMP files CAN GO IN EITHER DIRECTION and
    // the height will be negative for top-down.
    // Alse, there can be compression, etc. DONT think this is complete
    // BMP loading code because it isnt!!!


    // NOTE: Byte order in memory is determined by the Header itself.
    // So we have to read out the masks and convert the pixels ourselves.
    uint32 RedMask = Header->RedMask;
    uint32 GreenMask = Header->GreenMask;
    uint32 BlueMask = Header->BlueMask;
    uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

    bit_scan_result RedShift = FindLeastSignificantSetBit(RedMask);
    bit_scan_result GreenShift = FindLeastSignificantSetBit(GreenMask);
    bit_scan_result BlueShift = FindLeastSignificantSetBit(BlueMask);
    bit_scan_result AlphaShift = FindLeastSignificantSetBit(AlphaMask);

    Assert(RedShift.Found);
    Assert(GreenShift.Found);
    Assert(BlueShift.Found);
    Assert(AlphaShift.Found);

    uint32 *SourceDest = Pixels;
    for (int32 Y = 0; Y < Header->Height; ++Y) {
      for (int32 X = 0; X < Header->Width; ++X) {
        uint32 C = *SourceDest;
        *SourceDest++ = ((((C >> AlphaShift.Index) & 0xFF) << 24) | (((C >> RedShift.Index) & 0xFF) << 16) | (((C >> GreenShift.Index) & 0xFF) << 8) | (((C >> BlueShift.Index) & 0xFF) << 0));
      }
    }
  }

  return Result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

  real32 PlayerHeight = 1.4f;
  real32 PlayerWidth = 0.75f * PlayerHeight;

  game_state *GameState = (game_state *)Memory->PermanentStorage;

  if (!Memory->IsInitialized) {
    GameState->Backdrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");

    hero_bitmaps *Bitmap;

    Bitmap = GameState->HeroBitmaps;

    Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
    Bitmap->AlignX = 72;
    Bitmap->AlignY = 182;
    ++Bitmap;

    Bitmap->Head  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
    Bitmap->AlignX = 72;
    Bitmap->AlignY = 182;
    ++Bitmap;

    Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
    Bitmap->AlignX = 72;
    Bitmap->AlignY = 182;
    ++Bitmap;

    Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
    Bitmap->AlignX = 72;
    Bitmap->AlignY = 182;
    ++Bitmap;

    GameState->CameraP.AbsTileX = 17 / 2;
    GameState->CameraP.AbsTileY = 9 / 2;

    GameState->PlayerP.AbsTileX = 1;
    GameState->PlayerP.AbsTileY = 3;
    GameState->PlayerP.Offset.X = 5.0f;
    GameState->PlayerP.Offset.Y = 5.0f;

    InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8 *)Memory->PermanentStorage + sizeof(game_state));

    GameState->World = PushStruct(&GameState->WorldArena, world);
    world *World = GameState->World;
    World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

    tile_map *TileMap = World->TileMap;

    TileMap->ChunkShift = 4;
    TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
    TileMap->ChunkDim = (1 << TileMap->ChunkShift);

    TileMap->TileChunkCountX = 128;
    TileMap->TileChunkCountY = 128;
    TileMap->TileChunkCountZ = 2;
    TileMap->TileChunks = PushArray(&GameState->WorldArena, TileMap->TileChunkCountX * TileMap->TileChunkCountY * TileMap->TileChunkCountZ, tile_chunk);

    TileMap->TileSideInMeters = 1.4f;

    uint32 RandomNumberIndex = 0;
    uint32 TilesPerWidth = 17;
    uint32 TilesPerHeight = 9;
    uint32 ScreenX = 0;
    uint32 ScreenY = 0;
    uint32 AbsTileZ = 0;

    // TODO: Replace all this with real world generation!
    bool32 DoorLeft = false;
    bool32 DoorRight = false;
    bool32 DoorTop = false;
    bool32 DoorBottom = false;
    bool32 DoorUp = false;
    bool32 DoorDown = false;

    for (uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex) {
      // TODO: Random number generator!
      Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));

      uint32 RandomChoice;
      if (DoorUp || DoorDown) {
        RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
      } else {
        RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
      }

      bool32 CreatedZDoor = false;
      if (RandomChoice == 2) {
        CreatedZDoor = true;
        if (AbsTileZ == 0) {
          DoorUp = true;
        } else {
          DoorDown = true;
        }
      } else if (RandomChoice == 1) {
        DoorRight = true;
      } else {
        DoorTop = true;
      }

      for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY) {
        for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX) {
          uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
          uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

          uint32 TileValue = 1;
          if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight / 2)))) {
            TileValue = 2;
          }
          if ((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight / 2)))) {
            TileValue = 2;
          }
          if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth / 2)))) {
            TileValue = 2;
          }
          if ((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth / 2)))) {
            TileValue = 2;
          }
          if ((TileX == 10) && (TileY == 6)) {
            if (DoorUp) {
              TileValue = 3;
            }
            if (DoorDown) {
              TileValue = 4;
            }
          }

          SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ, TileValue);
        }
      }

      DoorLeft = DoorRight;
      DoorBottom = DoorTop;

      if (CreatedZDoor) {
        DoorDown = !DoorDown;
        DoorUp = !DoorUp;
      } else {
        DoorUp = false;
        DoorDown = false;
      }

      DoorRight = false;
      DoorTop = false;

      if (RandomChoice == 2) {
        if (AbsTileZ == 0) {
          AbsTileZ = 1;
        } else {
          AbsTileZ = 0;
        }
      } else if (RandomChoice == 1) {
        ScreenX += 1;
      } else {
        ScreenY += 1;
      }
    }

    Memory->IsInitialized = true;
  }

  world *World = GameState->World;
  tile_map *TileMap = World->TileMap;

  int32 TileSideInPixels = 60;
  real32 MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

  real32 LowerLeftX = -(real32)TileSideInPixels / 2;
  real32 LowerLeftY = (real32)Buffer->Height;

  for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    if (Controller->IsAnalog) {
      // NOTE: Use analog movement tuning
    } else {
      // NOTE: Use digital movement tuning
      // NOTE: ddPlayer is Player acceleration
      v2 ddPlayer = {};

      if (Controller->MoveUp.EndedDown) {
        GameState->FacingDirection = 1;
        ddPlayer.Y = 1.0f;
      }
      if (Controller->MoveDown.EndedDown) {
        GameState->FacingDirection = 3;
        ddPlayer.Y = -1.0f;
      }
      if (Controller->MoveLeft.EndedDown) {
        GameState->FacingDirection = 2;
        ddPlayer.X = -1.0f;
      }
      if (Controller->MoveRight.EndedDown) {
        GameState->FacingDirection = 0;
        ddPlayer.X = 1.0f;
      }
      if ((ddPlayer.X != 0.0f) && (ddPlayer.Y != 0.0f)) {
        ddPlayer *= 0.707106781187f;
      }

      real32 PlayerSpeed = 10.0f; // m/s^2
      if (Controller->ActionUp.EndedDown) {
        PlayerSpeed = 30.0f; // m/s^2
      }
      ddPlayer *= PlayerSpeed;

      tile_map_position NewPlayerP = GameState->PlayerP;

      // IMPORTANT: total hack, we need Ordinary Differential Equations to properly set friction
      ddPlayer += -1.5 * GameState->dPlayerP;

      // P' (position) = 1/2at² + vt + P
      NewPlayerP.Offset = (0.5f * ddPlayer * Square(Input->dtForFrame) +  GameState->dPlayerP * Input->dtForFrame + NewPlayerP.Offset);

      // P'' (velocity) = at + v
      GameState->dPlayerP = ddPlayer * Input->dtForFrame + GameState->dPlayerP;


      NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
      // TODO: Delta function that auto-recanonicalizes

      tile_map_position PlayerLeft = NewPlayerP;
      PlayerLeft.Offset.X -= 0.5f * PlayerWidth;
      PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

      tile_map_position PlayerRight = NewPlayerP;
      PlayerRight.Offset.X += 0.5f * PlayerWidth;
      PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

      if (IsTileMapPointEmpty(TileMap, NewPlayerP) && IsTileMapPointEmpty(TileMap, PlayerLeft) && IsTileMapPointEmpty(TileMap, PlayerRight)) {
        if (!AreOnSameTile(&GameState->PlayerP, &NewPlayerP)) {
          uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);

          if (NewTileValue == 3) {
            ++NewPlayerP.AbsTileZ;
          } else if (NewTileValue == 4) {
            --NewPlayerP.AbsTileZ;
          }
        }
        GameState->PlayerP = NewPlayerP;
      }

      GameState->CameraP.AbsTileZ = GameState->PlayerP.AbsTileZ;

      tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);

      if (Diff.dXY.X > (9.0f * TileMap->TileSideInMeters)) {
        GameState->CameraP.AbsTileX += 17;
      }
      if (Diff.dXY.X < -(9.0f * TileMap->TileSideInMeters)) {
        GameState->CameraP.AbsTileX -= 17;
      }

      // IMPORTANT: In the tutorial he puts 5f but 4.5f is better
      if (Diff.dXY.Y > (4.5f * TileMap->TileSideInMeters)) {
        GameState->CameraP.AbsTileY += 9;
      }
      if (Diff.dXY.Y < -(4.5f * TileMap->TileSideInMeters)) {
        GameState->CameraP.AbsTileY -= 9;
      }
    }
  }

  DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);

  real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;
  real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;

  // IMPORTANT: To center player and move camera, change CameraP for PlayerP

  for (int32 RelRow = -10; RelRow < 10; ++RelRow) {
    for (int32 RelColumn = -20; RelColumn < 20; ++RelColumn) {
      uint32 Column = GameState->CameraP.AbsTileX + RelColumn;
      uint32 Row = GameState->CameraP.AbsTileY + RelRow;
      uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);

      if (TileID > 1) {
        real32 Gray = 0.5f;
        if (TileID == 2) {
          Gray = 1.0f;
        }

        if (TileID > 2) {
          Gray = 0.25f;
        }

        if ((Column == GameState->CameraP.AbsTileX) && (Row == GameState->CameraP.AbsTileY)) {
          Gray = 0.0f;
        }

        v2 TileSide = {
          0.5f * TileSideInPixels,
          0.5f * TileSideInPixels
        };
        v2 Cen = {
          ScreenCenterX - MetersToPixels * GameState->CameraP.Offset.X + ((real32)RelColumn) * TileSideInPixels,
          ScreenCenterY + MetersToPixels * GameState->CameraP.Offset.Y - ((real32)RelRow) * TileSideInPixels
        };
        v2 Min = Cen - TileSide;
        v2 Max = Cen + TileSide;
        DrawRectangle(Buffer, Min, Max, Gray, Gray, Gray);
      }
    }
  }

  tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);

  real32 PlayerR = 1.0f;
  real32 PlayerG = 1.0f;
  real32 PlayerB = 0.0f;

  // IMPORTANT: To center player and move camera, PlayerGroundPointX/Y = ScreenCenterX/Y;
  real32 PlayerGroundPointX = ScreenCenterX + MetersToPixels * Diff.dXY.X;
  real32 PlayerGroundPointY = ScreenCenterY - MetersToPixels * Diff.dXY.Y;

  v2 PlayerLeftTop = {
    PlayerGroundPointX - 0.5f * MetersToPixels * PlayerWidth,
    PlayerGroundPointY - MetersToPixels * PlayerHeight
  };

  v2 PlayerWidthHeight = {
    PlayerWidth,
    PlayerHeight
  };

  DrawRectangle(Buffer, PlayerLeftTop, PlayerLeftTop + MetersToPixels * PlayerWidthHeight, PlayerR, PlayerG, PlayerB);

  hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[GameState->FacingDirection];
  DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
  DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
  DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  GameOutputSound(GameState, SoundBuffer, 400);
}
