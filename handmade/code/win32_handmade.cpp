#include <windows.h>
#include <stdint.h>

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

Win32WindowDimension Win32GetWindowDimension(HWND Window) {
  RECT ClientRect;
  GetClientRect(Window, &ClientRect); // gets the RECT of the drawable portion of the client`s window (it ignores borders and header bar which is controlled by Windows unless its on fullscreen)

  Win32WindowDimension Result;
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return Result;
}

internal void RenderWeirdGradient(Win32OffscreenBuffer Buffer, int BlueOffset, int GreenOffset)
{
  // TODO: pass a pointer or by value? Lets see what the optimizer does

  uint8 *Row = (uint8 *)Buffer.Memory;

  for (int Y = 0; Y < Buffer.Height; Y++)
  {
    uint32_t *Pixel = (uint32_t *)Row;

    for (int X = 0; X < Buffer.Width; X++)
    {
      uint8 Blue = (X + BlueOffset);
      uint8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue); // understand << (shift) operator
    }

    Row += Buffer.Pitch; // This could be Row += (uint8 *)Pixel because pixel will point to the end of the row after the for loop, but this is more explicit to understand
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

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, Win32OffscreenBuffer Buffer)
{
  // TODO: fix aspect ratio
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer.Width, Buffer.Height, Buffer.Memory, &Buffer.Info, DIB_RGB_COLORS, SRCCOPY);
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

  case WM_PAINT:
  {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    Win32WindowDimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackbuffer);
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
  // How to use message box
  // MessageBox(0, "Hello World", "Handmade Hero", MB_OK|MB_ICONINFORMATION);

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WNDCLASS WindowClass = {};
  WindowClass.style =  CS_HREDRAW | CS_VREDRAW;
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

        RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

        HDC DeviceContext = GetDC(Window);

        Win32WindowDimension Dimension = Win32GetWindowDimension(Window);

        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackbuffer);

        ReleaseDC(Window, DeviceContext);
        ++XOffset;
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
