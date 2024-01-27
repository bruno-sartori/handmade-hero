#include <windows.h>
#include <stdint.h>
#include <xinput.h>

// static keywords can have multiple behaviours in C++ so we create macros to highlight them
#define internal static
#define local_persist static
#define global_variable static

// typedef unsigned char uint8; // abreviation for unsigned char
typedef uint8_t uint8; // instead of creating the type abreviation for unsigned char in the line above stdint.h now has the uint8_t type that we could use
                       // but this dumbass wants to abreviate again to remove the _t so here... I will be doing this only on uint8 for education purposes

struct Win32OffscreenBuffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

struct Win32WindowDimension {
  int Width;
  int Height;
};

// TODO: this is global just for now
global_variable bool Running;
global_variable Win32OffscreenBuffer GlobalBackbuffer;

// =================================CONTROLLER FUNCTIONS====================================
// We are recreating the xinput.h functions definitions here so that it does not crash if Windows cannot load the library because of platform requirements issues
// As these functions can cause load errors we are not just loading the lib in build.bat as we`ve done before, instead we will load xinput with a function called Win32LoadXInput
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)

// XInputGetState
typedef X_INPUT_GET_STATE(X_Input_Get_State);
X_INPUT_GET_STATE(XInputGetStateStub) {
  return 0;
}
global_variable X_Input_Get_State *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
typedef X_INPUT_SET_STATE(X_Input_Set_State);
X_INPUT_SET_STATE(XInputSetStateStub) {
  return 0;
}
global_variable X_Input_Set_State *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
// =================================/CONTROLLER FUNCTIONS====================================

internal void Win32LoadXInput(void) {
  HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");

  if (XInputLibrary) {
    XInputGetState = (X_Input_Get_State *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState = (X_Input_Set_State *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}


internal Win32WindowDimension Win32GetWindowDimension(HWND Window) {
  RECT ClientRect;
  GetClientRect(Window, &ClientRect); // gets the RECT of the drawable portion of the client`s window (it ignores borders and header bar which is controlled by Windows unless its on fullscreen)

  Win32WindowDimension Result;
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return Result;
}

internal void RenderWeirdGradient(Win32OffscreenBuffer *Buffer, int BlueOffset, int GreenOffset)
{
  // TODO: pass a pointer or by value? Lets see what the optimizer does

  uint8 *Row = (uint8 *)Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; Y++)
  {
    uint32_t *Pixel = (uint32_t *)Row;

    for (int X = 0; X < Buffer->Width; X++)
    {
      uint8 Blue = (X + BlueOffset);
      uint8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue); // understand << (shift) operator
    }

    Row += Buffer->Pitch; // This could be Row += (uint8 *)Pixel because pixel will point to the end of the row after the for loop, but this is more explicit to understand
  }
}

// DIB stands for Device Independent Bitmap
internal void Win32ResizeDIBSection(Win32OffscreenBuffer *Buffer, int Width, int Height)
{
  // TODO: bulletproof this
  // Maybe dont free first, free after and then free first if that fails

  if (Buffer->Memory)
  {
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  // Pixels are always 32-bits wide (4 bytes), memory order BB GG RR XX
  int BytesPerPixel = 4;

  // When the bitHeight field is negative, this is the clue to Windows to treat this bitmap as top-down, not bottom-up, meaning that
  // the first three bytes of the image are the color for the top left pixel in the bitmap, not the bottom left. (see pitch and stride)
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

  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width * BytesPerPixel;

  // TODO: Probably clear this to black... why??
}

internal void Win32DisplayBufferInWindow(Win32OffscreenBuffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
  // TODO: fix aspect ratio
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
  LRESULT Result = 0;

  switch (Message)
  {
  case WM_SIZE:
  {
    OutputDebugStringA("WM_SIZE\n");
  }
  break;

  case WM_CLOSE:
  {
    // TODO: handle this with a message to the user?
    Running = false;
    OutputDebugStringA("WM_CLOSE\n");
  }
  break;

  case WM_ACTIVATEAPP:
  {
    OutputDebugStringA("WM_ACTIVATEAPP\n");
  }
  break;

  case WM_DESTROY:
  {
    // TODO: handle this as an error. Recreate window?
    Running = false;
    OutputDebugStringA("WM_DESTROY\n");
  }
  break;

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
  {
    uint32_t VKCode = WParam;
    bool WasDown = ((LParam & (1 << 30)) != 0);
    bool IsDown = ((LParam & (1 << 31)) == 0);

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
  } break;

  case WM_PAINT:
  {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    Win32WindowDimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
    EndPaint(Window, &Paint);
  }
  break;

  default:
  {
    // OutputDebugStringA("DEFAULT\n");
    Result = DefWindowProc(Window, Message, WParam, LParam);
  }
  break;
  }

  return Result;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  Win32LoadXInput();
  // How to use message box
  // MessageBox(0, "Hello World", "Handmade Hero", MB_OK|MB_ICONINFORMATION);

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WNDCLASSA WindowClass = {};
  WindowClass.style =  CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = hInstance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";
  // WindowClass.hIcon;

  if (RegisterClassA(&WindowClass))
  {
    HWND Window = CreateWindowExA(
        0,
        WindowClass.lpszClassName,
        "Handmade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        hInstance,
        0);

    if (Window)
    {
      // Since we specified CS_OWNDC, we can just get one device context and use it forever because we are not sharing it with anyone.
      HDC DeviceContext = GetDC(Window);

      int XOffset = 0;
      int YOffset = 0;

      Running = true;

      while (Running)
      {
        MSG Message;

        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
          if (Message.message == WM_QUIT)
          {
            Running = false;
          }

          TranslateMessage(&Message);
          DispatchMessageA(&Message); // Instead of handling messages here we need to dispatch it so that windows can preprocess it and then we handle it on our MainWindowCallback function
        }

        // TODO: should we poll this more frequently?
        for (DWORD ControllerIdx = 0; ControllerIdx < XUSER_MAX_COUNT; ControllerIdx++) { // Max is 4 controllers connected at the same time
          XINPUT_STATE ControllerState;
          if (XInputGetState(0, &ControllerState) == ERROR_SUCCESS) { // the controller is connected (wtf its called ERROR_SUCCESS???)
            // TODO: See if ControllerState.dwPacketNumber increments too rapidly
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
            bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
            bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
            bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
            bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
            bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
            bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

            int16_t StickX = Pad->sThumbLX;
            int16_t StickY = Pad->sThumbLY;

            XOffset += StickX >> 12;
            YOffset += StickY >> 12;
          } else {
            // Controller is not available
          }
        }

        /*
          XINPUT_VIBRATION Vibration;
          Vibration.wLeftMotorSpeed = 60000;
          Vibration.wRightMotorSpeed = 60000;
          XInputSetState(0, &Vibration);
        */

        RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

        Win32WindowDimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);

      }
    }
    else
    {
      // TODO: log
    }
  }
  else
  {
    // TODO: log
  }

  return (0);
}
