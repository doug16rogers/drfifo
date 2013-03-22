@rem build-x64.bat [DDK-version-dir [build-type]]
@rem DDK-version-dir should not contain a trailing \ character.
@rem build-type should be fre or chk.

set cwd=%cd%

if "%1"=="" goto No_Arg1
set DDK_DIR=%1
goto End_Arg1
:No_Arg1
set DDK_DIR=C:\WinDDK\7600.16385.1
:End_Arg1

if "%2"=="" goto No_Arg2
set DDK_BUILD_TYPE=%2
goto End_Arg2
:No_Arg2
set DDK_BUILD_TYPE=fre
:End_Arg2

call %DDK_DIR%\bin\setenv.bat %DDK_DIR%\ %DDK_BUILD_TYPE% x64 WIN7

cd %cwd%
%DDK_DIR%\bin\x86\amd64\build.exe /w 
