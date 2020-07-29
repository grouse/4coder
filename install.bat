@echo off

SET ROOT=%~dp0
SET BUILD_DIR=%ROOT%\build
SET CUSTOM_DIR=%ROOT%\custom
SET INSTALL_DIR=D:\apps\4coder

pushd %BUILD_DIR%
xcopy /Y /F custom_4coder.dll %INSTALL_DIR%\
xcopy /Y /F custom_4coder.pdb %INSTALL_DIR%\
xcopy /Y /F vc140.pdb %INSTALL_DIR%\
xcopy /Y /F config.4coder %INSTALL_DIR%\
xcopy /Y /F themes %INSTALL_DIR%\themes
xcopy /Y /F fonts %INSTALL_DIR%\fonts

popd
