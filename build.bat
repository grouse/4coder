@echo off

SET ROOT=%~dp0
SET BUILD_DIR="%ROOT%\build"
SET CUSTOM_DIR=%ROOT%\custom

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %BUILD_DIR%\themes mkdir %BUILD_DIR%\themes
if not exist %BUILD_DIR%\fonts mkdir %BUILD_DIR%\fonts

pushd %BUILD_DIR%
call %CUSTOM_DIR%\bin\buildsuper_x64.bat %ROOT%\custom.cpp debug
xcopy /Y /F %ROOT%\themes "%BUILD_DIR%\themes"
xcopy /Y /F %ROOT%\fonts "%BUILD_DIR%\fonts"
xcopy /Y /F %ROOT%\config.4coder "%BUILD_DIR%\"
popd

