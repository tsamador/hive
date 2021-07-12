@echo off

mkdir ..\..\build
pushd ..\..\build
cl /W4 -Oi -DHIVE_DEBUG=1 -FC -ZI ..\hive\code\win32_hive.cpp  user32.lib Gdi32.lib Dsound.lib winmm.lib
popd 
