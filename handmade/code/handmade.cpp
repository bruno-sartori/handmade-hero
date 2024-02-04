#include "handmade.h"

internal void GameOutputSound(GameState *State, GameSoundOutputBuffer *SoundBuffer) {
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / State->ToneHz;
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

    State->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;

    if (State->tSine > 2.0f * Pi32) {
      State->tSine -= 2.0f * Pi32;
    }
  }
}

internal void RenderWeirdGradient(GameOffscreenBuffer *Buffer, int BlueOffset, int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; Y++) {
    uint32 *Pixel = (uint32 *)Row;

    for (int X = 0; X < Buffer->Width; X++) {
      uint8 Blue = (uint8)(X + BlueOffset);
      uint8 Green = (uint8)(Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue);  // understand << (shift) operator
    }

    Row += Buffer->Pitch;
  }
}

internal void RenderPlayer(GameOffscreenBuffer *Buffer, int PlayerX, int PlayerY) {
  uint8 *EndOfBuffer = (uint8 *)Buffer->Memory + Buffer->Pitch * Buffer->Height;
  uint32 Color = 0xFFFFFFFF;
  int Top = PlayerY;
  int Bottom = PlayerY + 10;

  for (int X = PlayerX; X < PlayerX + 10; ++X) {
    uint8 *Pixel = ((uint8 *)Buffer->Memory + X * Buffer->BytesPerPixel + Top * Buffer->Pitch);

    for (int Y = Top; Y < Bottom; ++Y) {
      if ((Pixel >= Buffer->Memory) && ((Pixel + 4) <= EndOfBuffer)) {
        *(uint32 *)Pixel = Color;
      }

      Pixel += Buffer->Pitch;
    }
  }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
  Assert(sizeof(GameState) <= Memory->PermanentStorageSize);

  GameState *State = (GameState *)Memory->PermanentStorage;

  if (!Memory->IsInitialized) {
    char *Filename = __FILE__;

    DEBUGReadFileResult File = Memory->DEBUGPlatformReadEntireFile(Filename);

    if (File.Contents) {
      Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
      Memory->DEBUGPlatformFreeFileMemory(File.Contents);
    }

    State->ToneHz = 512;
    State->tSine = 0.0f;
    State->PlayerX = 500;
    State->PlayerY = 500;

    Memory->IsInitialized = true;
  }

  for (int ControllerIdx = 0; ControllerIdx < ArrayCount(Input->Controllers); ++ControllerIdx) {
    GameControllerInput *Controller = GetController(Input, ControllerIdx);

    if (Controller->IsAnalog) {
      // use analog movement tuning
      State->BlueOffset += (int)(4.0f * Controller->StickAverageX);
      State->ToneHz = 512 + (int)(128.0f * Controller->StickAverageY);
    } else {
      if (Controller->MoveLeft.EndedDown) {
        State->BlueOffset -= 1;
      }
      if (Controller->MoveRight.EndedDown) {
        State->BlueOffset += 1;
      }

      if (Controller->MoveUp.EndedDown) {
        State->GreenOffset += 1;
      }
      if (Controller->MoveDown.EndedDown) {
        State->GreenOffset -= 1;
      }
    }

    State->PlayerX += (int)(4.0f * Controller->StickAverageX);
    State->PlayerY -= (int)(4.0f * Controller->StickAverageY);
    if (State->tJump > 0) {
      State->PlayerY += (int)(10.0f * sinf(0.5f*Pi32*State->tJump));
    }

    if (Controller->ActionDown.EndedDown) {
      State->tJump = 4.0f;
    }
    State->tJump -= 0.033f;
  }

  RenderWeirdGradient(Buffer, State->BlueOffset, State->GreenOffset);
  RenderPlayer(Buffer, State->PlayerX, State->PlayerY);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  GameState *State = (GameState *)Memory->PermanentStorage;
  GameOutputSound(State, SoundBuffer);
}
