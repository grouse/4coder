@echo off

SET ROOT=%~dp0
SET BUILD_DIR=%ROOT%\build
SET CUSTOM_DIR=%ROOT%\4coder_base\custom
SET INSTALL_DIR=D:\apps\4coder

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

pushd %BUILD_DIR%
call %CUSTOM_DIR%\bin\buildsuper_x64-win.bat %ROOT%\custom.cpp release
xcopy /Y /F %ROOT%\themes %INSTALL_DIR%\themes
xcopy /Y /F %ROOT%\fonts %INSTALL_DIR%\fonts
xcopy /Y /F %ROOT%\config.4coder %INSTALL_DIR%\
xcopy /Y /F %ROOT%\bindings.4coder %INSTALL_DIR%\

xcopy /Y /F custom_4coder.dll %INSTALL_DIR%\
xcopy /Y /F custom_4coder.pdb %INSTALL_DIR%\

xcopy /Y /F %ROOT%\4coder_base\fonts "%INSTALL_DIR%\fonts"
xcopy /Y /F %ROOT%\4coder_base\4ed.exe "%INSTALL_DIR%\"
xcopy /Y /F %ROOT%\4coder_base\4ed_app.dll "%INSTALL_DIR%\"

popd

if %ERRORLEVEL% NEQ 0 (
	echo "ERROR"
)

pause