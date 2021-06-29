@echo off

mkdir ..\..\build
pushd ..\..\build
cl -DHIVE_DEBUG=1 /w -FC -ZI ..\hive\code\win32_hive.cpp  user32.lib Gdi32.lib Dsound.lib
popd 
