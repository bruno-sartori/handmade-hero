@echo off

set VcvarsallPath="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set CommomCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4456 -DINTERNAL=1 -DSLOW_PERFORMANCE=1 -DIS_WIN32=1 -Z7 -FC
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 32 Bit Build
REM call %VcvarsallPath% x86
REM cl %CommomCompilerFlags% ..\handmade\code\handmade.cpp -Fmhandmade.map /LD /link /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp  -Fmwin32_handmade.map /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64 Bit Build
call %VcvarsallPath% amd64
cl %CommomCompilerFlags% ..\handmade\code\handmade.cpp -Fmhandmade.map /LD /link /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl %CommomCompilerFlags% ..\handmade\code\win32_handmade.cpp  -Fmwin32_handmade.map /link %CommonLinkerFlags%

popd
