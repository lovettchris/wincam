@echo off
pushd %~dp0
nuget restore ScreenCapture.sln

msbuild /target:restore /p:Configuration=Release "/p:Platform=x64" ScreenCapture.sln
if ERRORLEVEL 1 goto :eof

msbuild /target:rebuild /p:Configuration=Release "/p:Platform=x64" ScreenCapture.sln

set TARGET=..\wincam\native\runtimes\x64
if not exist %TARGET% mkdir %TARGET%
copy /y x64\Release\ScreenCapture.* %TARGET%
popd