#include "handmade.h"

internal void GameOutputSound(GameState *State, GameSoundOutputBuffer *SoundBuffer) {
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / State->ToneHz;
  int16 *SampleOut = SoundBuffer->Samples;

  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
    real32 SineValue = sinf(State->tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);

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

    // Input.AButtonEndedDown;
    // Input.AButtonHalfTransitionCount;
  }

  RenderWeirdGradient(Buffer, State->BlueOffset, State->GreenOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  GameState *State = (GameState *)Memory->PermanentStorage;
  GameOutputSound(State, SoundBuffer);
}
