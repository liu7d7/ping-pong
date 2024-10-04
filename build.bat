::gcc -o art.exe art.c -I"C:\dev\raylib\include" -I"C:\dev\libwebsockets\build\include" -I"C:\dev\openssl\include" "C:\dev\raylib\lib\libraylib.a" "C:\dev\libwebsockets\build\bin\libwebsockets.dll" "C:\dev\openssl\lib\libssl.lib" -lopengl32 -lgdi32 -lwinmm -g
::  
::if %ERRORLEVEL% EQU 0 echo build success && .\art.exe

pushd build
cmake ..
ninja
popd

if %ERRORLEVEL% EQU 0 echo build success && .\build\art.exe
