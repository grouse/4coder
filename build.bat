@echo off

SET ROOT=%~dp0
SET BUILD_DIR=%ROOT%\build
SET CODE_HOME=%ROOT%\code

if NOT "%Platform%" == "X64" IF NOT "%Platform%" == "x64" (call "%CODE_HOME%\windows_scripts\setup_cl_x64.bat")

set FLAGS=/W4 /wd4310 /wd4100 /wd4201 /wd4505 /wd4996 /wd4127 /wd4510 /wd4512 /wd4610 /wd4457 /WX
set FLAGS=%FLAGS% /GR- /nologo /FC /Od /EHsc-
set DEBUG=/Zi
set BUILD_DLL=/LD /link /INCREMENTAL:NO /OPT:REF
set EXPORTS=/EXPORT:get_bindings /EXPORT:get_alpha_4coder_version

set SRC=%ROOT%/4coder_grouse.cpp

set PREPROC_FILE=4coder_command_metadata.i
set META_MACROS=/DMETA_PASS

pushd %BUILD_DIR%
cl /I"%CODE_HOME%" %FLAGS% %DEBUG% %SRC% /P /Fi%PREPROC_FILE% %META_MACROS%
cl /I"%CODE_HOME%" %FLAGS% %DEBUG% "%CODE_HOME%\4coder_metadata_generator.cpp" /Femetadata_generator
metadata_generator -R "%CODE_HOME%" "%cd%\\%PREPROC_FILE%"
cl /I"%CODE_HOME%" %FLAGS% %DEBUG% %SRC% /Fecustom_4coder %BUILD_DLL% %EXPORTS%

REM file spammation preventation
del metadata_generator*
del *.exp
del *.obj
del *.lib
del %PREPROC_FILE%

popd

