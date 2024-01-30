#include "handmade.h"
/*
DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
int16* SampleOut = (int16*)Region1;
*/

internal void GameOutputSound(GameSoundOutputBuffer *SoundBuffer, int ToneHz) {
  local_persist real32 tSine;
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
  int16 *SampleOut = SoundBuffer->Samples;

  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
    real32 SineValue = sinf(tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);

    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

    tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
  }
}

internal void RenderWeirdGradient(GameOffscreenBuffer* Buffer, int BlueOffset, int GreenOffset) {
  // TODO: pass a pointer or by value? Lets see what the optimizer does

  uint8* Row = (uint8*)Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; Y++) {
    uint32* Pixel = (uint32*)Row;

    for (int X = 0; X < Buffer->Width; X++) {
      uint8 Blue = (X + BlueOffset);
      uint8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue);  // understand << (shift) operator
    }

    Row += Buffer->Pitch;
  }
}

internal void GameUpdateAndRender(GameMemory *Memory, GameInput *Input, GameOffscreenBuffer* Buffer, GameSoundOutputBuffer *SoundBuffer) {
  Assert(sizeof(GameState) <= Memory->PermanentStorageSize);

  GameState* State = (GameState*)Memory->PermanentStorage;

  if (!Memory->IsInitialized) {
    State->ToneHz = 256;
    Memory->IsInitialized = true;
  }

  GameControllerInput *Input0 = &Input->Controllers[0];

  if (Input0->IsAnalog) {
    // use analog movement tuning
    State->BlueOffset += (int)4.0f*(Input0->EndX);
    State->ToneHz = 256 + (int)(128.0f*(Input0->EndY));
  } else {
    // use digital movement tuning
  }

  // Input.AButtonEndedDown;
  // Input.AButtonHalfTransitionCount;

  if (Input0->Down.EndedDown) {
    State->GreenOffset +=1;
  }


  // TODO: allow sample offsets here for more robust platform options
  GameOutputSound(SoundBuffer, State->ToneHz);
  RenderWeirdGradient(Buffer, State->BlueOffset, State->GreenOffset);
}

