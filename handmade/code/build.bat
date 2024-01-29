@echo off

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -Zi -FC ..\handmade\code\win32_handmade.cpp user32.lib gdi32.lib
popd
