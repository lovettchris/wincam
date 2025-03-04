@echo off

if "%ffmpegPath%"=="" goto :noffmpeg

pushd %~dp0
nuget restore ScreenCapture.sln

msbuild /target:restore /p:Configuration=Release "/p:Platform=x64" ScreenCapture.sln
if ERRORLEVEL 1 goto :eof

msbuild /target:rebuild /p:Configuration=Release "/p:Platform=x64" ScreenCapture.sln

set TARGET=..\wincam\native\runtimes\x64
if not exist %TARGET% mkdir %TARGET%
copy /y x64\Release\ScreenCapture.* %TARGET%
copy /y "%ffmpegPath%\*.dll" %TARGET%
popd
goto :eof

:noffmpeg
echo Please set your ffmpegPath to point to your ffmpeg binaries
exit /b 1