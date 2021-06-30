@echo off

mkdir ..\..\build
pushd ..\..\build
cl -Oi -DHIVE_DEBUG=1 -FC -ZI ..\hive\code\win32_hive.cpp  user32.lib Gdi32.lib Dsound.lib
popd 
