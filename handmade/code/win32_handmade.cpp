#include <windows.h>

// static keywords can have multiple behaviours in C++ so we create macros to highlight them
#define internal static
#define local_persist static
#define global_variable static

// TODO: this is global just for now
global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;

// DIB stands for Device Independent Bitmap
internal void Win32ResizeDIBSection(int Width, int Height)
{
  // TODO: bulletproof this
  // Maybe dont free first, free after and then free first if that fails

  // Free our DIBSection
  if (BitmapHandle) {
    DeleteObject(BitmapHandle);
  }

  if (!BitmapDeviceContext) {
    // TODO: should we recreate these under certain special circumstances
    BitmapDeviceContext = CreateCompatibleDC(0);
  }

  BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
  BitmapInfo.bmiHeader.biWidth = Width;
  BitmapInfo.bmiHeader.biHeight = Height;
  BitmapInfo.bmiHeader.biPlanes = 1;
  BitmapInfo.bmiHeader.biBitCount = 32;
  BitmapInfo.bmiHeader.biCompression = BI_RGB;

  BitmapHandle = CreateDIBSection(
      BitmapDeviceContext,
      &BitmapInfo,
      DIB_RGB_COLORS,
      &BitmapMemory,
      0,
      0);
}

internal void Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
  StretchDIBits(DeviceContext, X, Y, Width, Height, X, Y, Width, Height, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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

    Win32UpdateWindow(DeviceContext, X, Y, Width, Height);
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
    HWND WindowHandle = CreateWindowExA(
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

    if (WindowHandle)
    {
      Running = true;

      while (Running)
      {
        MSG Message;
        BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);

        if (MessageResult > 0)
        {
          TranslateMessage(&Message);

          // Instead of handling messages here we need to dispatch it so that windows can preprocess it and then
          // we handle it on our MainWindowCallback function
          DispatchMessageA(&Message);
        }
        else
        {
          break;
        }
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
