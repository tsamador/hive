@echo off

mkdir ..\..\build
pushd ..\..\build
cl /w -FC -ZI ..\hive\code\win32_hive.cpp  user32.lib Gdi32.lib Dsound.lib
win32_hive.exe
popd 
