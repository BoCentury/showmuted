@echo off

set CL=-W4 -WX -wd4100 -wd4189 -wd4201 -wd4505
set CL=%CL% -Oi -Gm- -GR- -EHa- -nologo -FC -fp:fast -fp:except- /std:c++latest -Zc:strictStrings-
set LINK= -incremental:no -opt:ref   user32.lib gdi32.lib dbghelp.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd %~dp0..\build

if "%1" == "release" (
    echo RELEASE
	del showmuted.exe
    cl -O2 -MT -D_RELEASE_BUILD=1 -Fe:showmuted.exe ..\code\win32_first.cpp
) else (
    cl -Od -MTd -Z7 -D_INTERNAL_BUILD=1 -Fe:showmuted.exe  ..\code\win32_first.cpp
)

popd

exit /b %errorlevel%