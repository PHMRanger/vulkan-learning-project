@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

mkdir build
pushd build

SET ARGS=/MD /Zi /W4 /EHsc

SET LIBS=glfw3.lib user32.lib gdi32.lib shell32.lib vulkan-1.lib
SET LIBPATHS=/LIBPATH:..\thirdparty\glfw\lib-vc2017 /LIBPATH:C:\VulkanSDK\1.1.114.0\Lib
SET INCLUDES=/I ..\thirdparty\glfw\include /I C:\VulkanSDK\1.1.114.0\Include

SET SRCS= 

cl %ARGS% %INCLUDES% ..\src\main.c %SRCS% /link %LIBPATHS% %LIBS%

:: del *.obj *.ilk *.pdb

popd