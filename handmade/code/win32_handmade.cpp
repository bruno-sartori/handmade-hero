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

#include <math.h>  // TODO: implement sin function ourselves
#include <stdint.h>

// ================================ TYPES AND GLOBALS HERE
#include <dsound.h>
#include <malloc.h>
#include <stdio.h>
#include <windows.h>
#include <xinput.h>

#include "handmade.cpp"
#include "win32_handmade.h"

// TODO: this is global just for now
global_variable bool32 Running;
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

internal DEBUGReadFileResult DEBUGPlatformReadEntireFile(CHAR* Filename) {
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
          DEBUGPlatformFreeFileMemory(Result.Contents);
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

internal void DEBUGPlatformFreeFileMemory(void* Memory) {
  if (Memory) {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

internal bool32 DEBUGPlatformWriteEntireFile(char* Filename, uint32 MemorySize, void* Memory) {
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

  // StretchDIBits will be replaced by BitBlt

  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;

  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * Buffer->BytesPerPixel;

  // TODO: Probably clear this to black... why??
}

internal void Win32DisplayBufferInWindow(Win32OffscreenBuffer* Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight) {
  // TODO: fix aspect ratio
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
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
  Assert(NewState->EndedDown != IsDown);

  NewState->EndedDown = IsDown;
  ++NewState->HalfTransitionCount;
}

internal void Win32ProcessPendingMessages(GameControllerInput* KeyboardController) {
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

internal void Win32DebugDrawVertical(Win32OffscreenBuffer *Backbuffer, int X, int Top, int Bottom, uint32 Color) {
  uint8 *Pixel = ((uint8 *)Backbuffer->Memory + X * Backbuffer->BytesPerPixel + Top * Backbuffer->Pitch);

  for (int Y = Top; Y < Bottom; ++Y) {
    *(uint32 *)Pixel = Color;
    Pixel += Backbuffer->Pitch;
  }
}

inline void Win32DrawSoundBufferMarker(Win32OffscreenBuffer *Backbuffer, Win32SoundOutput *SoundOutput, real32 C, int PadX, int Top, int Bottom, DWORD Value, uint32 Color) {
  Assert(Value < SoundOutput->SecondaryBufferSize);
  real32 XReal32 = (real32)(C * (real32)Value);
  int X = PadX + (int)XReal32;
  Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(Win32OffscreenBuffer *Backbuffer, int MarkerCount, Win32DebugTimeMarker *Markers, Win32SoundOutput *SoundOutput, real32 TargetSecondsPerFrame) {
  int PadX = 16;
  int PadY = 16;

  int Top = PadY;
  int Bottom = Backbuffer->Height - PadY;

  real32 C = (real32)(Backbuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;

  for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex) {
    Win32DebugTimeMarker *ThisMarker = &Markers[MarkerIndex];
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->PlayCursor, 0xFFFFFFFF);
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->WriteCursor, 0xFFFF0000);
  }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  // Set the Windows schedules granularity to 1ms so that our Sleep() can be more granular
  UINT DesiredScheduleMs = 1;
  bool32 SleepIsGranular = (timeBeginPeriod(DesiredScheduleMs) == TIMERR_NOERROR);

  Win32LoadXInput();

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WNDCLASSA WindowClass = {};
  WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = hInstance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";
  // WindowClass.hIcon;

#define FamesOfAudioLatency 3
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)

  real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

  if (RegisterClassA(&WindowClass)) {
    HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);

    if (Window) {
      // Since we specified CS_OWNDC, we can just get one device context
      // and use it forever because we are not sharing it with anyone.
      HDC DeviceContext = GetDC(Window);

      Win32SoundOutput SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.BytesPerSample = sizeof(int16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = FamesOfAudioLatency * (SoundOutput.SamplesPerSecond / GameUpdateHz);

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

      uint64 TotalSize = Memory.PermanentStorageSize + Memory.TransientStorageSize;
      Memory.PermanentStorage = VirtualAlloc(BaseAddress, (SIZE_T)TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      Memory.TransientStorage = ((uint8*)Memory.PermanentStorage + Memory.PermanentStorageSize);

      if (Samples && Memory.PermanentStorage && Memory.TransientStorage) {
        GameInput Input[2] = {};
        GameInput* NewInput = &Input[0];
        GameInput* OldInput = &Input[1];

        LARGE_INTEGER LastCounter = Win32GetWallClock();

        int DebugTimeMarkerIndex = 0;
        Win32DebugTimeMarker DebugTimeMarkers[GameUpdateHz / 2] = {0};

        // TODO: Handle startup specially
        DWORD LastPlayCursor = 0;
        bool32 SoundIsValid = false;

        uint64 LastCycleCount = __rdtsc();

        while (Running) {
          // TODO: Zeroing macro
          // TODO: cannot zero everything because the up/down state will be wrong
          GameControllerInput* OldKeyboardController = GetController(OldInput, 0);
          GameControllerInput* NewKeyboardController = GetController(NewInput, 0);
          *NewKeyboardController = {};  // para zerar os valores de KeyboardController
          NewKeyboardController->IsConnected = true;

          for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex) {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
          }

          Win32ProcessPendingMessages(NewKeyboardController);

          DWORD MaxControllerCount = XUSER_MAX_COUNT;
          if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
            MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
          }

          for (DWORD ControllerIdx = 0; ControllerIdx < MaxControllerCount; ControllerIdx++) {
            DWORD OurControllerIdx = ControllerIdx + 1;
            GameControllerInput* OldController = GetController(OldInput, OurControllerIdx);
            GameControllerInput* NewController = GetController(NewInput, OurControllerIdx);

            XINPUT_STATE ControllerState;
            if (XInputGetState(0, &ControllerState) == ERROR_SUCCESS) {
              NewController->IsConnected = true;
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

          // NOTE: Compute how much sound to write and where
          DWORD ByteToLock = 0;
          DWORD TargetCursor = 0;
          DWORD BytesToWrite = 0;
          if (SoundIsValid) {
            ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
            TargetCursor = ((LastPlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize);

            if (ByteToLock > TargetCursor) {
              BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
              BytesToWrite += TargetCursor;
            } else {
              BytesToWrite = TargetCursor - ByteToLock;
            }
          }

          GameSoundOutputBuffer SoundBuffer = {};
          SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
          SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
          SoundBuffer.Samples = Samples;

          GameOffscreenBuffer Buffer = {};
          Buffer.Memory = GlobalBackbuffer.Memory;
          Buffer.Width = GlobalBackbuffer.Width;
          Buffer.Height = GlobalBackbuffer.Height;
          Buffer.Pitch = GlobalBackbuffer.Pitch;
          GameUpdateAndRender(&Memory, NewInput, &Buffer, &SoundBuffer);

          if (SoundIsValid) {

#if INTERNAL
            DWORD PlayCursor;
            DWORD WriteCursor;
            SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

            char TextBuffer[256];
            sprintf_s(TextBuffer, sizeof(TextBuffer), "LPC: %u BTL: %u TC: %u BTW: %u - PC: %u WC: %u \n", LastPlayCursor, ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor);
            OutputDebugStringA(TextBuffer);
#endif

            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
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
            // Assert(TestSecondsElapsedForFrame < TargetSecondsPerFrame);

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
#if INTERNAL
          Win32DebugSyncDisplay(&GlobalBackbuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, &SoundOutput, TargetSecondsPerFrame);
#endif
          Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);

          DWORD PlayCursor;
          DWORD WriteCursor;
          if (SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) {
            LastPlayCursor = PlayCursor;

            if (!SoundIsValid) {
              SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
              SoundIsValid = true;
            }
          } else {
            SoundIsValid = false;
          }

#if INTERNAL
          // NOTE: This is debug code
          {
            Win32DebugTimeMarker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex++];

            if (DebugTimeMarkerIndex > ArrayCount(DebugTimeMarkers)) {
              DebugTimeMarkerIndex = 0;
            }

            Marker->PlayCursor = PlayCursor;
            Marker->WriteCursor = WriteCursor;
          }
#endif

          GameInput* Temp = NewInput;
          NewInput = OldInput;
          OldInput = Temp;

          uint64 EndCycleCount = __rdtsc();
          uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
          LastCycleCount = EndCycleCount;

          real64 FPS = 0.0f; //(real64)GlobalPerfCountFrequency / (real64)CounterElapsed;
          real64 MegaCyclesPerFrame = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

          char OutputBuffer[256];
          sprintf_s(OutputBuffer, "ms/frame: %.2fms / FPS: %.2f / MC/frame: %.2f \n", MsPerFrame, FPS, MegaCyclesPerFrame);
          OutputDebugStringA(OutputBuffer);
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
