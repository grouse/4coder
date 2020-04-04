@echo off

SET ROOT=%~dp0
SET BUILD_DIR=%ROOT%\build
SET CUSTOM_DIR=%ROOT%\custom

pushd %BUILD_DIR%
call %CUSTOM_DIR%\bin\buildsuper_x64.bat %ROOT%\custom.cpp release
xcopy /Y /F %ROOT%\theme-grouse.4coder %BUILD_DIR%\themes\
xcopy /Y /F %ROOT%\config.4coder %BUILD_DIR%\
popd

