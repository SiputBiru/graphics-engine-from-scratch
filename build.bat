@echo off

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

SET includes=/Isrc /I%VULKAN_SDK%/Include
SET links=/link /LIBPATH:%VULKAN_SDK%/Lib vulkan-1.lib user32.lib
SET defines=/D DEBUG /D WINDOWS_BUILD

echo "Compiling Shaders..."

@REM for /r %%i in ("assets\shaders\*") do (
@REM     if NOT "%%~xi" == ".spv" (
@REM         echo compiling %%i
@REM         "C:\VulkanSDK\1.3.296.0\Bin\glslc.exe" %%i -o %%i.spv
@REM     )
@REM     wait
@REM )

echo "Building main.."

cl /EHsc /Z7 /Fe"main" %includes% %defines% src/platform/win32_platform.cpp %links%
