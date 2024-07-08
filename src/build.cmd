@echo off

cd ~dp0

msbuild /target:restore /p:Configuration=Release "/p:Platform=x64" ScreenCapture.sln
if ERRORLEVEL 1 goto :eof

msbuild /target:rebuild /p:Configuration=Release "/p:Platform=x64" ScreenCapture.sln

