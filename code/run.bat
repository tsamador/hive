@echo off

mkdir ..\..\build
pushd ..\..\build
call build.bat
win32_hive.exe
popd 
