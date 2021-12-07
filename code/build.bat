@echo off

mkdir ..\..\build
pushd ..\..\build
cl -Oi -DHIVE_DEBUG=1 -FC -ZI ..\hive\code\gl_hive.cpp ..\hive\code\glad.c ..\hive\code\ShaderLoader.cpp /link  /LIBPATH:..\Learn_OpenGl\lib user32.lib Gdi32.lib Dsound.lib winmm.lib OpenGL32.lib glfw3.lib Shell32.lib
popd 
