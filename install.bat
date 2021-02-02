@echo off

SET ROOT=%~dp0
SET BUILD_DIR=%ROOT%\build
SET CUSTOM_DIR=%ROOT%\custom
SET INSTALL_DIR=D:\apps\4coder

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %BUILD_DIR%\themes mkdir %BUILD_DIR%\themes
if not exist %BUILD_DIR%\fonts mkdir %BUILD_DIR%\fonts


pushd %BUILD_DIR%
call %CUSTOM_DIR%\bin\buildsuper_x64-win.bat %ROOT%\custom.cpp release
xcopy /Y /F %ROOT%\themes %BUILD_DIR%\themes
xcopy /Y /F %ROOT%\fonts %BUILD_DIR%\fonts
xcopy /Y /F %ROOT%\config.4coder %BUILD_DIR%\
xcopy /Y /F %ROOT%\bindings.4coder %BUILD_DIR%\

xcopy /Y /F custom_4coder.dll %INSTALL_DIR%\
xcopy /Y /F custom_4coder.pdb %INSTALL_DIR%\
xcopy /Y /F vc140.pdb %INSTALL_DIR%\
xcopy /Y /F config.4coder %INSTALL_DIR%\
xcopy /Y /F bindings.4coder %INSTALL_DIR%\
xcopy /Y /F themes %INSTALL_DIR%\themes
xcopy /Y /F fonts %INSTALL_DIR%\fonts

popd
