#include <windows.h>
#include <stdint.h>

// static keywords can have multiple behaviours in C++ so we create macros to highlight them
#define internal static
#define local_persist static
#define global_variable static

// typedef unsigned char uint8; // abreviation for unsigned char
typedef uint8_t uint8; // instead of creating the type abreviation for unsigned char in the line above stdint.h now has the uint8_t type that we could use
                       // but this dumbass wants to abreviate again to remove the _t so here... I will be doing this only on uint8 for education purposes

// TODO: this is global just for now
global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void RenderWeirdGradient(int XOffset, int YOffset)
{
  int Width = BitmapWidth;
  int Height = BitmapHeight;

  int Pitch = Width * BytesPerPixel;
  uint8 *Row = (uint8 *)BitmapMemory;

  for (int Y = 0; Y < BitmapHeight; Y++)
  {
    uint32_t *Pixel = (uint32_t *)Row;

    for (int X = 0; X < BitmapWidth; X++)
    {
      /*
                          0  1  2  3
        Pixel in memory: 00 00 00 00
                         BB GG RR XX
                         0x xxRRGGBB
        its written backwards in memory so that it normalizes when it goes to the CPU registers


      // BLUE
      *Pixel++ = (uint8)(X + XOffset); // assigns a value (color) to the pointer (pixel)
      ++Pixel;                       // increments the memory address of the pointer

      // GREEN
      *Pixel++ = (uint8)(Y + YOffset);
      ++Pixel;

      // RED
      *Pixel = 0;
      ++Pixel;

      // PADDING
      *Pixel = 0;
      ++Pixel;
      */

     /*
        Memory:    BB GG RR xx
        Register:  xx RR GG BB

        Pixel: (32-bits)
     */

      uint8 Blue = (X + XOffset);
      uint8 Green = (Y + YOffset);

      *Pixel++ = ((Green << 8) | Blue); // understand << (shift) operator
    }

    Row += Pitch;
  }
}

// DIB stands for Device Independent Bitmap
internal void Win32ResizeDIBSection(int Width, int Height)
{
  // TODO: bulletproof this
  // Maybe dont free first, free after and then free first if that fails

  if (BitmapMemory)
  {
    VirtualFree(BitmapMemory, 0, MEM_RELEASE);
  }

  BitmapWidth = Width;
  BitmapHeight = Height;

  BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
  BitmapInfo.bmiHeader.biWidth = BitmapWidth;
  BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // negative because we will be saving image as top-down (see pitch and stride)
  BitmapInfo.bmiHeader.biPlanes = 1;
  BitmapInfo.bmiHeader.biBitCount = 32;
  BitmapInfo.bmiHeader.biCompression = BI_RGB;

  // StretchDIBits will be replaced by BitBlt

  int BitmapMemorySize = (BitmapWidth * BitmapHeight) * BytesPerPixel;
  BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

  // TODO: Probably clear this to black... why??
}

internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
  int WindowWidth = ClientRect->right - ClientRect->left;
  int WindowHeight = ClientRect->bottom - ClientRect->top;

  StretchDIBits(DeviceContext, 0, 0, BitmapWidth, BitmapHeight, 0, 0, WindowWidth, WindowHeight, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
  LRESULT Result = 0;

  switch (Message)
  {
  case WM_SIZE:
  {
    RECT ClientRect;
    GetClientRect(Window, &ClientRect); // gets the RECT of the drawable portion of the client`s window (it ignores borders and header bar which is controlled by Windows unless its on fullscreen)

    int Width = ClientRect.right - ClientRect.left;
    int Height = ClientRect.bottom - ClientRect.top;

    Win32ResizeDIBSection(Width, Height);
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

    int X = Paint.rcPaint.left;
    int Y = Paint.rcPaint.top;
    int Width = Paint.rcPaint.right - Paint.rcPaint.left;
    int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect); // gets the RECT of the drawable portion of the client`s window (it ignores borders and header bar which is controlled by Windows unless its on fullscreen)

    Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
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

  WNDCLASS WindowClass = {};
  WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
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

        RenderWeirdGradient(XOffset, YOffset);

        HDC DeviceContext = GetDC(Window);

        RECT ClientRect;
        GetClientRect(Window, &ClientRect); // gets the RECT of the drawable portion of the client`s window (it ignores borders and header bar which is controlled by Windows unless its on fullscreen)


        int WindowWidth = ClientRect.right - ClientRect.left;
        int WindowHeight = ClientRect.bottom - ClientRect.top;

        Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);

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
