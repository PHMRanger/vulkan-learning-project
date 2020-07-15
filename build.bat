@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

mkdir build
pushd build

SET ARGS=/MD /Zi /W4 /EHsc

SET LIBS=glfw3.lib user32.lib gdi32.lib shell32.lib vulkan-1.lib fmt.lib
SET LIBPATHS=/LIBPATH:..\thirdparty\glfw\lib-vc2017 /LIBPATH:C:\VulkanSDK\1.2.141.2\Lib /LIBPATH:..\thirdparty\fmt\lib
SET INCLUDES=/I ..\thirdparty\glfw\include /I C:\VulkanSDK\1.2.141.2\Include /I ..\thirdparty\fmt\include /I ..\thirdparty\linmath\include

SET SRCS=

cl /DDEBUG %ARGS% %INCLUDES% ..\src\main.cpp %SRCS% /link %LIBPATHS% %LIBS%

:: del *.obj *.ilk *.pdb

popd