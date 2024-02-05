/*
  TODO: THIS IS NOT A FINAL PLATFORM LAYER

  - Saved game locations
  - Getting a handle to our own executable file
  - Asset loading path
  - Threading (launch a thread)
  - Raw Input (support multiple keyboards)
  - Sleep/timeBeginPeriod
  - ClipCursor() (multimonitor support)
  - Fullscreen support
  - WM_SETCURSOR (control cursor visibility)
  = QUeryCancelAutoplay
  - WM_ACTIVATEAPP (for when we are not the active application)
  - Blit speed improvements (BitBlit)
  - Hardware Acceleration (OpenGL or Direct3D or both)
  - GetKeyboardLayout (for french keyboards, international WASD support)

  Just a partial list of stuff!!
*/
#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO: this is global just for now
global_variable bool32 Running;
global_variable bool32 Pause;
global_variable Win32OffscreenBuffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER SecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

// =================================CONTROLLER FUNCTIONS====================================
// We are recreating the xinput.h
// functions definitions here so that it does not crash if Windows cannot load
// the library because of platform requirements issues As these functions can
// cause load errors we are not just loading the lib in build.bat as we`ve done
// before, instead we will load xinput with a function called Win32LoadXInput
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)

// XInputGetState
typedef X_INPUT_GET_STATE(X_Input_Get_State);
X_INPUT_GET_STATE(XInputGetStateStub) {
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable X_Input_Get_State* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
typedef X_INPUT_SET_STATE(X_Input_Set_State);
X_INPUT_SET_STATE(XInputSetStateStub) {
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable X_Input_Set_State* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
// =================================/CONTROLLER FUNCTIONS====================================

// =================================SOUND FUNCTIONS ========================================
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)  // Why the fuck this time he doesnt write the other two statements?
typedef DIRECT_SOUND_CREATE(Direct_Sound_Create);                                                                      // Tutorial 07 - 30min
// =================================/SOUND FUNCTIONS ========================================

internal void CatStrings(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB, size_t DestCount, char *Dest) {
  for(int Index = 0; Index < SourceACount; ++Index) {
    *Dest++ = *SourceA++;
  }

  for(int Index = 0; Index < SourceBCount; ++Index) {
    *Dest++ = *SourceB++;
  }

  *Dest++ = 0;
}

internal void Win32GetEXEFileName(Win32State *PlatformState) {
  // NOTE: Never use MAX_PATH in code that is user-facing, because it can be
  // dangerous and lead to bad results.
  DWORD SizeOfFileName = GetModuleFileNameA(0, PlatformState->EXEFileName, sizeof(PlatformState->EXEFileName));
  PlatformState->OnePastLastEXEFileNameSlash = PlatformState->EXEFileName;
  for (char* Scan = PlatformState->EXEFileName; *Scan; ++Scan) {
    if (*Scan == '\\') {
      PlatformState->OnePastLastEXEFileNameSlash = Scan + 1;
    }
  }
}

internal int StringLength(char *String) {
  int Count = 0;
  while (*String++) {
    ++Count;
  }
  return Count;
}

internal void Win32BuildEXEPathFileName(Win32State *PlatformState, char *FileName, int DestCount, char *Dest) {
  CatStrings(PlatformState->OnePastLastEXEFileNameSlash - PlatformState->EXEFileName, PlatformState->EXEFileName, StringLength(FileName), FileName, DestCount, Dest);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
  if (Memory) {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  DEBUGReadFileResult Result = {};

  HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  if (FileHandle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER FileSize;

    if (GetFileSizeEx(FileHandle, &FileSize)) {
      uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
      Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      if (Result.Contents) {
        DWORD BytesRead;
        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead)) {
          // File read successfully
          Result.ContentsSize = FileSize32;
        } else {
          DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
          Result.Contents = 0;
        }
      } else {
        // TODO: Logging
      }
    } else {
      // TODO: Logging
    }

    CloseHandle(FileHandle);
  } else {
    // TODO: Logging
  }

  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
  bool32 Result = false;
  HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

  if (FileHandle != INVALID_HANDLE_VALUE) {
    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) {
      Result = BytesWritten == MemorySize;
    } else {
      // TODO: Logging
    }

    CloseHandle(FileHandle);
  } else {
    // TODO: Logging
  }

  return Result;
}

inline FILETIME GetLastWriteTime(char *FileName) {
  FILETIME LastWriteTime = {};

  WIN32_FILE_ATTRIBUTE_DATA Data;
  if (GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data)) {
    LastWriteTime = Data.ftLastWriteTime;
  }

  return LastWriteTime;
}

internal Win32GameCode Win32LoadGameCode(char *SourceDLLName, char *TempDLLName) {
  Win32GameCode Result = {};

  Result.DLLLastWriteTime = GetLastWriteTime(SourceDLLName);
  CopyFile(SourceDLLName, TempDLLName, FALSE);
  Result.GameCodeDLL = LoadLibraryA(TempDLLName);

  if (Result.GameCodeDLL) {
    Result.UpdateAndRender = (Game_Update_And_Render*)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
    Result.GetSoundSamples = (Game_Get_Sound_Samples*)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
    Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
  }

  if (!Result.IsValid) {
    Result.UpdateAndRender = 0;
    Result.GetSoundSamples = 0;
  }

  return Result;
}

internal void Win32UnloadGameCode(Win32GameCode* GameCode) {
  if (GameCode->GameCodeDLL) {
    FreeLibrary(GameCode->GameCodeDLL);
    GameCode->GameCodeDLL = 0;
  }

  GameCode->IsValid = false;
  GameCode->UpdateAndRender = 0;
  GameCode->GetSoundSamples = 0;
}

internal void Win32LoadXInput(void) {
  // TODO: test this on Windows 8
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

  if (!XInputLibrary) {
    XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
  }
  if (!XInputLibrary) {
    XInputLibrary = LoadLibraryA("xinput1_3.dll");
  }

  if (XInputLibrary) {
    XInputGetState = (X_Input_Get_State*)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState = (X_Input_Set_State*)GetProcAddress(XInputLibrary, "XInputSetState");
  } else {
    // TODO: Diagnostic
  }
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize) {
  // Load the library
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

  if (DSoundLibrary) {
    // Get a DirectSound object
    Direct_Sound_Create* DirectSoundCreate = (Direct_Sound_Create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

    // TODO: Double check if this works on XP - DirectSound8 or 7??
    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
      WaveFormat.cbSize = 0;

      // Create a primary buffer
      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
        // BufferDescription.dwBufferBytes = 0;

        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
          if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
            // We finally set the format
            OutputDebugStringA("Primary buffer format was set.\n");
          } else {
            // TODO: Diagnosis
          }
        } else {
          // TODO: Diagnostic
        }
      } else {
        // TODO: Diagnostic
      }

      // create a secondary buffer
      DSBUFFERDESC SecondaryBufferDescription = {};
      SecondaryBufferDescription.dwSize = sizeof(SecondaryBufferDescription);
      SecondaryBufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
      SecondaryBufferDescription.dwBufferBytes = BufferSize;
      SecondaryBufferDescription.lpwfxFormat = &WaveFormat;

      if (SUCCEEDED(DirectSound->CreateSoundBuffer(&SecondaryBufferDescription, &SecondaryBuffer, 0))) {
        // Start it playing!
        OutputDebugStringA("Secondary buffer created successfully.\n");
      }

    } else {
      // TODO: Diagnostic
    }
  } else {
    // TODO: Diagnostic
  }
}

internal Win32WindowDimension Win32GetWindowDimension(HWND Window) {
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);

  Win32WindowDimension Result;
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return Result;
}

internal void Win32ResizeDIBSection(Win32OffscreenBuffer* Buffer, int Width, int Height) {
  // TODO: bulletproof this
  // Maybe dont free first, free after and then free first if that fails

  if (Buffer->Memory) {
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  // Pixels are always 32-bits wide (4 bytes), memory order BB GG RR XX

  // When the bitHeight field is negative, this is the clue to Windows to
  // treat this bitmap as top-down, not bottom-up, meaning that the first
  // three bytes of the image are the color for the top left pixel in the
  // bitmap, not the bottom left. (see pitch and stride)
  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->BytesPerPixel = 4;
  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;

  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(Win32OffscreenBuffer* Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight) {
  // NOTE: For prototyping purposes, we`re going to always blit 1-to-1 pixels to
  // make sure we dont introduce artifacts with stretching while we are learning
  // to code the renderer
  StretchDIBits(DeviceContext, 0, 0, Buffer->Width, Buffer->Height, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
  LRESULT Result = 0;

  switch (Message) {
    case WM_SIZE: {
      OutputDebugStringA("WM_SIZE\n");
    } break;

    case WM_CLOSE: {
      // TODO: handle this with a message to the user?
      OutputDebugStringA("WM_CLOSE\n");
      Running = false;
    } break;

    case WM_ACTIVATEAPP: {
      /*if (WParam == TRUE) {
        SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
      } else {
        SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA);
      }
*/
      OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_DESTROY: {
      // TODO: handle this as an error. Recreate window?
      OutputDebugStringA("WM_DESTROY\n");
      Running = false;
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      Assert(!"Keyboard input came in through a non-dispatch message!");
    } break;

    case WM_PAINT: {
      PAINTSTRUCT Paint;
      HDC DeviceContext = BeginPaint(Window, &Paint);
      Win32WindowDimension Dimension = Win32GetWindowDimension(Window);
      Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
      EndPaint(Window, &Paint);
    } break;

    default: {
      // OutputDebugStringA("DEFAULT\n");
      Result = DefWindowProcA(Window, Message, WParam, LParam);
    } break;
  }

  return Result;
}

internal void Win32ClearSoundBuffer(Win32SoundOutput* SoundOutput) {
  VOID* Region1;
  DWORD Region1Size;
  VOID* Region2;
  DWORD Region2Size;

  if (SUCCEEDED(SecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
    uint8* DestSample = (uint8*)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
      *DestSample++ = 0;
    }

    DestSample = (uint8*)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
      *DestSample++ = 0;
    }

    SecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

internal void Win32FillSoundBuffer(Win32SoundOutput* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, GameSoundOutputBuffer* SourceBuffer) {
  // int16 int16  int16 int16  int16 int16  ...
  // [LEFT RIGHT] [LEFT RIGHT] [LEFT RIGHT] ...
  VOID* Region1;
  DWORD Region1Size;
  VOID* Region2;
  DWORD Region2Size;

  if (SUCCEEDED(SecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
    int16* SourceSample = SourceBuffer->Samples;

    int16* DestSample = (int16*)Region1;
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }

    DestSample = (int16*)Region2;
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }

    SecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, GameButtonState* OldState, GameButtonState* NewState, DWORD ButtonBit) {
  NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
  NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(GameButtonState* NewState, bool32 IsDown) {
  if(NewState->EndedDown != IsDown) {
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
  }
}

internal void Win32GetInputFileLocation(Win32State *PlatformState, bool32 InputStream, int SlotIndex, int DestCount, char *Dest) {
  char Temp[64];
  wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
  Win32BuildEXEPathFileName(PlatformState, Temp, DestCount, Dest);
}

internal Win32ReplayBuffer *Win32GetReplayBuffer(Win32State *PlatformState, int unsigned Index) {
  Assert(Index < ArrayCount(PlatformState->ReplayBuffers));
  Win32ReplayBuffer *Result = &PlatformState->ReplayBuffers[Index];
  return Result;
}

internal void Win32BeginRecordingInput(Win32State *PlatformState, int InputRecordingIndex) {
  Win32ReplayBuffer *ReplayBuffer = Win32GetReplayBuffer(PlatformState, InputRecordingIndex);

  if (ReplayBuffer->MemoryBlock) {
    PlatformState->InputRecordingIndex = InputRecordingIndex;

    char FileName[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(PlatformState, true, InputRecordingIndex, sizeof(FileName), FileName);
    PlatformState->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
    LARGE_INTEGER FilePosition;
    FilePosition.QuadPart = PlatformState->TotalSize;
    SetFilePointerEx(PlatformState->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
    CopyMemory(ReplayBuffer->MemoryBlock, PlatformState->GameMemoryBlock, PlatformState->TotalSize);
  }
}

internal void Win32EndRecordingInput(Win32State *PlatformState) {
  CloseHandle(PlatformState->RecordingHandle);
  PlatformState->InputRecordingIndex = 0;

}

internal void Win32BeginInputPlayback(Win32State *PlatformState, int InputPlayingIndex) {
  Win32ReplayBuffer *ReplayBuffer = Win32GetReplayBuffer(PlatformState, InputPlayingIndex);

  if (ReplayBuffer->MemoryBlock) {
    PlatformState->InputPlayingIndex = InputPlayingIndex;
    char FileName[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(PlatformState, true, InputPlayingIndex, sizeof(FileName), FileName);
    PlatformState->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
    LARGE_INTEGER FilePosition;
    FilePosition.QuadPart = PlatformState->TotalSize;
    SetFilePointerEx(PlatformState->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif
    CopyMemory(PlatformState->GameMemoryBlock, ReplayBuffer->MemoryBlock, PlatformState->TotalSize);
  }
}

internal void Win32EndInputPlayback(Win32State *PlatformState) {
  CloseHandle(PlatformState->PlaybackHandle);
  PlatformState->InputPlayingIndex = 0;
}

internal void Win32RecordInput(Win32State *PlatformState, GameInput *NewInput) {
  DWORD BytesWritten;
  WriteFile(PlatformState->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void Win32PlayBackInput(Win32State *PlatformState, GameInput *NewInput) {
  DWORD BytesRead;
  if(ReadFile(PlatformState->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0)) {
    if (BytesRead == 0) {
      // NOTE: We`ve hit the end of the stream, go back to the beginning
      int PlayingIndex = PlatformState->InputPlayingIndex;
      Win32EndInputPlayback(PlatformState);
      Win32BeginInputPlayback(PlatformState, PlayingIndex);
      ReadFile(PlatformState->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
    }
  }
}

internal void Win32ProcessPendingMessages(Win32State *PlatformState, GameControllerInput* KeyboardController) {
  MSG Message;
  while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
    switch (Message.message) {
      case WM_QUIT: {
        Running = false;
      } break;

      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP: {
        uint32 VKCode = (uint32)Message.wParam;
        bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
        bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);

        if (WasDown != IsDown) {
          if (VKCode == 'W') {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
          } else if (VKCode == 'A') {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
          } else if (VKCode == 'S') {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
          } else if (VKCode == 'D') {
            Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
          } else if (VKCode == 'Q') {
            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
          } else if (VKCode == 'E') {
            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
          } else if (VKCode == VK_UP) {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
          } else if (VKCode == VK_LEFT) {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
          } else if (VKCode == VK_DOWN) {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
          } else if (VKCode == VK_RIGHT) {
            Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
          } else if (VKCode == VK_SPACE) {
            Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
          } else if (VKCode == VK_ESCAPE) {
            Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
          }
#if INTERNAL
          else if (VKCode == 'P') {
            if (IsDown) {
              Pause = !Pause;
            }
          } else if (VKCode == 'L') {
            if (IsDown) {
              if (PlatformState->InputPlayingIndex == 0) {
                if (PlatformState->InputRecordingIndex == 0) {
                  Win32BeginRecordingInput(PlatformState, 1);
                } else {
                  Win32EndRecordingInput(PlatformState);
                  Win32BeginInputPlayback(PlatformState, 1);
                }
              } else {
                Win32EndInputPlayback(PlatformState);
              }
            }
          }
#endif
        }

        bool32 AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
        if ((VKCode == VK_F4) && AltKeyWasDown) {
          Running = false;
        }
      } break;
      default: {
        TranslateMessage(&Message);
        DispatchMessageA(&Message);
      } break;
    }
  }
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold) {
  real32 Result = 0;

  if (Value < -DeadZoneThreshold) {
    Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
  } else if (Value > DeadZoneThreshold) {
    Result = (real32)((Value + DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
  }

  return Result;
}

inline LARGE_INTEGER Win32GetWallClock() {
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);
  return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
  real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency);
  return Result;
}

#if 0
internal void Win32DebugDrawVertical(Win32OffscreenBuffer* Backbuffer, int X, int Top, int Bottom, uint32 Color) {
  if (Top <= 0) {
    Top = 0;
  }
  if (Bottom > Backbuffer->Height) {
    Bottom = Backbuffer->Height;
  }

  if ((X >= 0) && (X < Backbuffer->Width)) {
    uint8* Pixel = ((uint8*)Backbuffer->Memory + X * Backbuffer->BytesPerPixel + Top * Backbuffer->Pitch);

    for (int Y = Top; Y < Bottom; ++Y) {
      *(uint32*)Pixel = Color;
      Pixel += Backbuffer->Pitch;
    }
  }
}

inline void Win32DrawSoundBufferMarker(Win32OffscreenBuffer* Backbuffer, Win32SoundOutput* SoundOutput, real32 C, int PadX, int Top, int Bottom, DWORD Value, uint32 Color) {
  real32 XReal32 = (real32)(C * (real32)Value);
  int X = PadX + (int)XReal32;

  Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}


internal void Win32DebugSyncDisplay(Win32OffscreenBuffer* Backbuffer, int MarkerCount, Win32DebugTimeMarker* Markers, int CurrentMarkerIndex, Win32SoundOutput* SoundOutput, real32 TargetSecondsPerFrame) {
  int PadX = 16;
  int PadY = 16;

  int LineHeight = 64;

  real32 C = (real32)(Backbuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;

  for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex) {
    Win32DebugTimeMarker* ThisMarker = &Markers[MarkerIndex];
    Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

    DWORD PlayColor = 0xFFFFFFFF;
    DWORD WriteColor = 0xFFFF0000;
    DWORD ExpectedFlipColor = 0xFFFFFF00;
    DWORD PlayWindowColor = 0XFFFF00FF;

    int Top = PadY;
    int Bottom = PadY + LineHeight;

    if (MarkerIndex == CurrentMarkerIndex) {
      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      int FirstTop = Top;
      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
    }

    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample, PlayWindowColor);
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
  }
}
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
  Win32State PlatformState = {};

  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  Win32GetEXEFileName(&PlatformState);

  char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
  Win32BuildEXEPathFileName(&PlatformState, "handmade.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
  char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
  Win32BuildEXEPathFileName(&PlatformState, "handmade_temp.dll", sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);


  // Set the Windows schedules granularity to 1ms so that our Sleep() can be more granular
  UINT DesiredScheduleMs = 1;
  bool32 SleepIsGranular = (timeBeginPeriod(DesiredScheduleMs) == TIMERR_NOERROR);

  Win32LoadXInput();

  WNDCLASSA WindowClass = {};

  Win32ResizeDIBSection(&GlobalBackbuffer, 960, 540);

  WindowClass.style = CS_HREDRAW | CS_VREDRAW;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = hInstance;
  // WindowClass.hIcon;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClassA(&WindowClass)) {
    HWND Window = CreateWindowExA(0 /*WS_EX_TOPMOST|WS_EX_LAYERED*/, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);

    if (Window) {
      int MonitorRefreshHz = 60;
      HDC RefreshDC = GetDC(Window);
      int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
      ReleaseDC(Window, RefreshDC);
      if (Win32RefreshRate > 1) {
        MonitorRefreshHz = Win32RefreshRate;
      }
      real32 GameUpdateHz = (MonitorRefreshHz / 2.0f);
      real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

      Win32SoundOutput SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.BytesPerSample = sizeof(int16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.SafetyBytes = (int)(((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample / GameUpdateHz) / 3.0f);

      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearSoundBuffer(&SoundOutput);
      SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      Running = true;

#if 0
      // NOTE: This tests the PlayCursor/WriteCursor update frequency
      // On the Handmade Hero machine, it was 480 samples
      while (Running) {
        DWORD PlayCursor;
        DWORD WriteCursor;
        if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
          char TextBuffer[256];
          sprintf_s(TextBuffer, sizeof(TextBuffer), "PC:%u WC: %u\n", PlayCursor, WriteCursor);
          OutputDebugStringA(TextBuffer);
        }
      }
#endif
      // Pool with bitmap VirtualAlloc
      int16* Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if INTERNAL
      LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
      LPVOID BaseAddress = 0;
#endif

      GameMemory Memory = {};
      Memory.PermanentStorageSize = Megabytes(64);
      Memory.TransientStorageSize = Gigabytes(1);
      Memory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
      Memory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
      Memory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

      PlatformState.TotalSize = Memory.PermanentStorageSize + Memory.TransientStorageSize;
      PlatformState.GameMemoryBlock = VirtualAlloc(BaseAddress, (SIZE_T)PlatformState.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      Memory.PermanentStorage = PlatformState.GameMemoryBlock;
      Memory.TransientStorage = ((uint8*)Memory.PermanentStorage + Memory.PermanentStorageSize);

      for(int ReplayIndex = 0; ReplayIndex < ArrayCount(PlatformState.ReplayBuffers); ++ReplayIndex) {
        Win32ReplayBuffer* ReplayBuffer = &PlatformState.ReplayBuffers[ReplayIndex];

        Win32GetInputFileLocation(&PlatformState, false, ReplayIndex, sizeof(ReplayBuffer->FileName), ReplayBuffer->FileName);

        ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->FileName, GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);

        LARGE_INTEGER MaxSize;
        MaxSize.QuadPart = PlatformState.TotalSize;
        ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE, MaxSize.HighPart, MaxSize.LowPart, 0);
        DWORD Error = GetLastError();
        ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, PlatformState.TotalSize);

        if(ReplayBuffer->MemoryBlock){
        } else {
          // TODO: Diagnostic
        }
      }

      if (Samples && Memory.PermanentStorage && Memory.TransientStorage) {
        GameInput Input[2] = {};
        GameInput* NewInput = &Input[0];
        GameInput* OldInput = &Input[1];

        LARGE_INTEGER LastCounter = Win32GetWallClock();
        LARGE_INTEGER FlipWallClock = Win32GetWallClock();

        int DebugTimeMarkerIndex = 0;
        Win32DebugTimeMarker DebugTimeMarkers[30] = {0};

        DWORD AudioLatencyBytes = 0;
        real32 AudioLatencySeconds = 0;
        bool32 SoundIsValid = false;

        Win32GameCode Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

        uint64 LastCycleCount = __rdtsc();

        while (Running) {
          NewInput->dtForFrame = TargetSecondsPerFrame;

          FILETIME NewDLLWriteTime = GetLastWriteTime(SourceGameCodeDLLFullPath);
          if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0) {
            Win32UnloadGameCode(&Game);
            Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
          }
          // TODO: Zeroing macro
          // TODO: cannot zero everything because the up/down state will be wrong
          GameControllerInput* OldKeyboardController = GetController(OldInput, 0);
          GameControllerInput* NewKeyboardController = GetController(NewInput, 0);
          *NewKeyboardController = {};  // para zerar os valores de KeyboardController
          NewKeyboardController->IsConnected = true;

          for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex) {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
          }

          Win32ProcessPendingMessages(&PlatformState, NewKeyboardController);

          if (!Pause) {
            POINT MouseP;
            GetCursorPos(&MouseP);
            ScreenToClient(Window, &MouseP);
            NewInput->MouseX = MouseP.x;
            NewInput->MouseY = MouseP.y;
            NewInput->MouseZ = 0; // TODO: Support mousewheel?
            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

            DWORD MaxControllerCount = XUSER_MAX_COUNT;
            if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
              MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
            }

            for (DWORD ControllerIdx = 0; ControllerIdx < MaxControllerCount; ControllerIdx++) {
              DWORD OurControllerIdx = ControllerIdx + 1;
              GameControllerInput* OldController = GetController(OldInput, OurControllerIdx);
              GameControllerInput* NewController = GetController(NewInput, OurControllerIdx);

              XINPUT_STATE ControllerState;
              if (XInputGetState(ControllerIdx, &ControllerState) == ERROR_SUCCESS) {
                NewController->IsConnected = true;
                NewController->IsAnalog = OldController->IsAnalog;
                // TODO: See if ControllerState.dwPacketNumber
                // increments too rapidly
                XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                if (NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f) {
                  NewController->IsAnalog = true;
                }

                if ((Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)) {
                  NewController->StickAverageY = 1.0f;
                  NewController->IsAnalog = false;
                }
                if ((Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)) {
                  NewController->StickAverageY = -1.0f;
                  NewController->IsAnalog = false;
                }
                if ((Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)) {
                  NewController->StickAverageX = -1.0f;
                  NewController->IsAnalog = false;
                }
                if ((Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)) {
                  NewController->StickAverageX = 1.0f;
                  NewController->IsAnalog = false;
                }

                real32 Threshold = 0.5f;
                Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, &OldController->MoveLeft, &NewController->MoveLeft, 1);
                Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0, &OldController->MoveRight, &NewController->MoveRight, 1);
                Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, &OldController->MoveDown, &NewController->MoveDown, 1);
                Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0, &OldController->MoveUp, &NewController->MoveUp, 1);

                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, &NewController->ActionDown, XINPUT_GAMEPAD_A);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, &NewController->ActionRight, XINPUT_GAMEPAD_B);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, &NewController->ActionLeft, XINPUT_GAMEPAD_X);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, &NewController->ActionUp, XINPUT_GAMEPAD_Y);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, &NewController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, &NewController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, &NewController->Start, XINPUT_GAMEPAD_START);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, &NewController->Back, XINPUT_GAMEPAD_BACK);
              } else {
                // NOTE: The controller is not available
                NewController->IsConnected = false;
              }
            }

            ThreadContext Thread = {};

            GameOffscreenBuffer Buffer = {};
            Buffer.Memory = GlobalBackbuffer.Memory;
            Buffer.Width = GlobalBackbuffer.Width;
            Buffer.Height = GlobalBackbuffer.Height;
            Buffer.Pitch = GlobalBackbuffer.Pitch;
            Buffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;

            if (PlatformState.InputRecordingIndex) {
              Win32RecordInput(&PlatformState, NewInput);
            }

            if (PlatformState.InputPlayingIndex) {
              Win32PlayBackInput(&PlatformState, NewInput);
            }

            if (Game.UpdateAndRender) {
              // IMPORTANT: Here we call the game update and render
              Game.UpdateAndRender(&Thread, &Memory, NewInput, &Buffer);
            }

            LARGE_INTEGER AudioWallClock = Win32GetWallClock();
            real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

            DWORD PlayCursor;
            DWORD WriteCursor;
            if (SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) {
              /* NOTE:
                Here is how sound output computation works.

                We define a safety value that is the number of samples we think our game update loop may vary
                by (let's say up to 2ms).

                When we wake up to write audio, we will look and see what the play cursor position is and we
                will forecast ahead where we think the play cursor will be on the next frame boundary.

                We will then look to see if the write cursor is before that by at least our safety value. If
                it is, the target fill position is that frame boundary plus one frame. This gives us perfect
                audio sync in the case of a card that has low enough latency.

                If the write cursor is AFTER that safety margin, then we assume we can never sync the audio
                perfectly, so we will write one frame's worth of audio plus the safety margin`s worth
                of guard samples.
              */
              if (!SoundIsValid) {
                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                SoundIsValid = true;
              }

              DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

              DWORD ExpectedSoundBytesPerFrame = (int)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);
              real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
              DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);
              DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

              DWORD SafeWriteCursor = WriteCursor;
              if (SafeWriteCursor < PlayCursor) {
                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
              }
              Assert(SafeWriteCursor >= PlayCursor);
              SafeWriteCursor += SoundOutput.SafetyBytes;
              bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

              DWORD TargetCursor = 0;
              if (AudioCardIsLowLatency) {
                TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
              } else {
                TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
              }
              TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

              DWORD BytesToWrite = 0;
              if (ByteToLock > TargetCursor) {
                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                BytesToWrite += TargetCursor;
              } else {
                BytesToWrite = TargetCursor - ByteToLock;
              }

              GameSoundOutputBuffer SoundBuffer = {};
              SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
              SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
              SoundBuffer.Samples = Samples;

              if (Game.GetSoundSamples) {
                // IMPORTANT: Here we call the game get sound samples
                Game.GetSoundSamples(&Thread, &Memory, &SoundBuffer);
              }

#if INTERNAL
              Win32DebugTimeMarker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
              Marker->OutputPlayCursor = PlayCursor;
              Marker->OutputWriteCursor = WriteCursor;
              Marker->OutputLocation = ByteToLock;
              Marker->OutputByteCount = BytesToWrite;
              Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

              DWORD UnwrappedWriteCursor = WriteCursor;
              if (UnwrappedWriteCursor < PlayCursor) {
                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
              }

              AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
              AudioLatencySeconds = ((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond;
#if 0
              char TextBuffer[256];
              sprintf_s(TextBuffer, sizeof(TextBuffer), "BTL: %u TC: %u BTW: %u - PC: %u WC: %u DELTA: %u (%fs) \n", ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
              OutputDebugStringA(TextBuffer);
#endif
#endif

              Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
            } else {
              SoundIsValid = false;
            }

            LARGE_INTEGER WorkCounter = Win32GetWallClock();
            real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

            real32 SecondsElapsedForFrame = WorkSecondsElapsed;
            if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
              if (SleepIsGranular) {
                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));

                if (SleepMS > 0) {
                  Sleep(SleepMS);
                }
              }

              real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

              if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) {
                // TODO: Log Missed sleep here
                OutputDebugString("Missed sleep\n");
              }

              while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
              }
            } else {
              // TODO: MISSED FRAME RATE
            }

            LARGE_INTEGER EndCounter = Win32GetWallClock();
            real64 MsPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
            LastCounter = EndCounter;

            Win32WindowDimension Dimension = Win32GetWindowDimension(Window);

            HDC DeviceContext = GetDC(Window);
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            ReleaseDC(Window, DeviceContext);

            FlipWallClock = Win32GetWallClock();
#if INTERNAL
            // NOTE: This is debug code
            {
              DWORD PlayCursor;
              DWORD WriteCursor;
              if (SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) {
                Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));

                Win32DebugTimeMarker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];

                Marker->FlipPlayCursor = PlayCursor;
                Marker->FlipWriteCursor = WriteCursor;
              }
            }
#endif

            GameInput* Temp = NewInput;
            NewInput = OldInput;
            OldInput = Temp;
#if 1
            uint64 EndCycleCount = __rdtsc();
            uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
            LastCycleCount = EndCycleCount;

            real64 FPS = 0.0f;  //(real64)GlobalPerfCountFrequency / (real64)CounterElapsed;
            real64 MegaCyclesPerFrame = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

            char OutputBuffer[256];
            sprintf_s(OutputBuffer, "ms/frame: %.2fms / FPS: %.2f / MC/frame: %.2f \n", MsPerFrame, FPS, MegaCyclesPerFrame);
            OutputDebugStringA(OutputBuffer);
#endif
#if INTERNAL
            ++DebugTimeMarkerIndex;

            if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers)) {
              DebugTimeMarkerIndex = 0;
            }
#endif
          }
        }
      } else {
        // TODO: log
      }
    } else {
      // TODO: log
    }
  } else {
    // TODO: log
  }

  return (0);
}
