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
      SecondaryBufferDescription.dwFlags = 0;
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
  GetClientRect(
      Window,
      &ClientRect);  // gets the RECT of the drawable portion of the client`s
                     // window (it ignores borders and header bar which is
                     // controlled by Windows unless its on fullscreen)

  Win32WindowDimension Result;
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return Result;
}

// DIB stands for Device Independent Bitmap
internal void Win32ResizeDIBSection(Win32OffscreenBuffer* Buffer, int Width, int Height) {
  // TODO: bulletproof this
  // Maybe dont free first, free after and then free first if that fails

  if (Buffer->Memory) {
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  // Pixels are always 32-bits wide (4 bytes), memory order BB GG RR XX
  int BytesPerPixel = 4;

  // When the bitHeight field is negative, this is the clue to Windows to
  // treat this bitmap as top-down, not bottom-up, meaning that the first
  // three bytes of the image are the color for the top left pixel in the
  // bitmap, not the bottom left. (see pitch and stride)
  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  // StretchDIBits will be replaced by BitBlt

  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;

  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * BytesPerPixel;

  // TODO: Probably clear this to black... why??
}

internal void Win32DisplayBufferInWindow(Win32OffscreenBuffer* Buffer,
                                         HDC DeviceContext,
                                         int WindowWidth,
                                         int WindowHeight) {
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
      Running = false;
      OutputDebugStringA("WM_CLOSE\n");
    } break;

    case WM_ACTIVATEAPP: {
      OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_DESTROY: {
      // TODO: handle this as an error. Recreate window?
      Running = false;
      OutputDebugStringA("WM_DESTROY\n");
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      uint32 VKCode = WParam;
      bool32 WasDown = ((LParam & (1 << 30)) != 0);
      bool32 IsDown = ((LParam & (1 << 31)) == 0);

      if (WasDown != IsDown) {
        if (VKCode == 'W') {
          if (IsDown) {
            OutputDebugStringA("W Pressed\n");
          }

          if (WasDown) {
            OutputDebugStringA("W Released\n");
          }
        } else if (VKCode == 'A') {
          OutputDebugStringA("A Pressed\n");
        } else if (VKCode == 'S') {
          OutputDebugStringA("S Pressed\n");
        } else if (VKCode == 'D') {
          OutputDebugStringA("D Pressed\n");
        } else if (VKCode == 'Q') {
          OutputDebugStringA("Q Pressed\n");
        } else if (VKCode == 'E') {
          OutputDebugStringA("E Pressed\n");
        } else if (VKCode == VK_SPACE) {
          OutputDebugStringA("Space Pressed\n");
        } else if (VKCode == VK_ESCAPE) {
          OutputDebugStringA("ESC Pressed\n");
        }
      }

      bool32 AltKeyWasDown = ((LParam & (1 << 29)) != 0);
      if ((VKCode == VK_F4) && AltKeyWasDown) {
        Running = false;
      }
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  Win32LoadXInput();

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WNDCLASSA WindowClass = {};
  WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = hInstance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";
  // WindowClass.hIcon;

  if (RegisterClassA(&WindowClass)) {
    HWND Window = CreateWindowExA(
        0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);

    if (Window) {
      // Since we specified CS_OWNDC, we can just get one device context
      // and use it forever because we are not sharing it with anyone.
      HDC DeviceContext = GetDC(Window);

      Win32SoundOutput SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.BytesPerSample = sizeof(int16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearSoundBuffer(&SoundOutput);
      SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      Running = true;

      // Pool with bitmap VirtualAlloc
      int16* Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      GameInput Input[2] = {};
      GameInput *NewInput = &Input[0];
      GameInput *OldInput = &Input[1];

      uint64 LastCycleCount = __rdtsc();
      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);

      while (Running) {
        MSG Message;

        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
          if (Message.message == WM_QUIT) {
            Running = false;
          }

          TranslateMessage(&Message);
          DispatchMessageA(&Message);
          // Instead of handling messages here we need
          // to dispatch it so that windows can
          // preprocess it and then we handle it on
          // our MainWindowCallback function
        }

        int MaxControllerCount = XUSER_MAX_COUNT;
        if (MaxControllerCount > ArrayCount(NewInput->Controllers)) {
          MaxControllerCount = ArrayCount(NewInput->Controllers);
        }

        for (DWORD ControllerIdx = 0; ControllerIdx < MaxControllerCount; ControllerIdx++) {
          GameControllerInput *OldController = &OldInput->Controllers[ControllerIdx];
          GameControllerInput *NewController = &NewInput->Controllers[ControllerIdx];

          XINPUT_STATE ControllerState;
          if (XInputGetState(0, &ControllerState) == ERROR_SUCCESS) {  // the controller is connected (wtf
            // its called ERROR_SUCCESS???)
            // TODO: See if ControllerState.dwPacketNumber
            // increments too rapidly
            XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

            bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

            NewController->IsAnalog = true;
            NewController->StartX = OldController->EndX;
            NewController->StartY = OldController->EndY;

            real32 X;
            if (Pad->sThumbLX < 0) {
              X = (real32)Pad->sThumbLX / 32768.0f;
            } else {
              X = (real32)Pad->sThumbLX / 32767.0f;
            }
            NewController->MinX = OldController->MaxX = NewController->EndX = X;

            real32 Y;
            if (Pad->sThumbLY < 0) {
              Y = (real32)Pad->sThumbLY / 32768.0f;
            } else {
              Y = (real32)Pad->sThumbLY / 32767.0f;
            }
            NewController->MinY = OldController->MaxY = NewController->EndY = Y;

            // TODO: Handle deadzone later using
            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE = 7849
            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE = 8689

            // Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->State, NewController->State, XINPUT_GAMEPAD_START);
            // Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->State, NewController->State, XINPUT_GAMEPAD_BACK);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, &NewController->Down, XINPUT_GAMEPAD_A);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, &NewController->Right, XINPUT_GAMEPAD_B);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, &NewController->Left, XINPUT_GAMEPAD_X);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, &NewController->Up, XINPUT_GAMEPAD_Y);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, &NewController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, &NewController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER);
          } else {
            // Controller is not available
          }
        }

        // DirectSound output test
        DWORD ByteToLock = 0;
        DWORD TargetCursor = 0;
        DWORD BytesToWrite = 0;
        DWORD PlayCursor = 0;
        DWORD WriteCursor = 0;
        bool32 SoundIsValid = false;

        if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
          ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
          TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize);

          if (ByteToLock > TargetCursor) {
            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
            BytesToWrite += TargetCursor;
          } else {
            BytesToWrite = TargetCursor - ByteToLock;
          }

          SoundIsValid = true;
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
        GameUpdateAndRender(NewInput, &Buffer, &SoundBuffer);

        if (SoundIsValid) {
          Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        }

        Win32WindowDimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);

        uint64 EndCycleCount = __rdtsc();
        LARGE_INTEGER EndCounter;
        QueryPerformanceCounter(&EndCounter);

        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
        int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
        real64 MsPerFrame = (((1000.0f * (real64)CounterElapsed) / (real64)PerfCountFrequency));
        real64 FPS = (real64)PerfCountFrequency / (real64)CounterElapsed;
        real64 MegaCyclesPerFrame = ((real64)CyclesElapsed / (1000.0f * 1000.0f));
#if 0
        char OutputBuffer[256];
        sprintf(OutputBuffer, "ms/frame: %.3fms / FPS: %.3f / MC/frame: %.3f \n", MsPerFrame, FPS, MegaCyclesPerFrame);
        OutputDebugStringA(OutputBuffer);
#endif
        LastCycleCount = EndCycleCount;
        LastCounter = EndCounter;

        GameInput *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
      }
    } else {
      // TODO: log
    }
  } else {
    // TODO: log
  }

  return (0);
}
