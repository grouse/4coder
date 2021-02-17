@echo off

SET ROOT=%~dp0
SET BUILD_DIR="%ROOT%\build"
SET CUSTOM_DIR=%ROOT%\4coder_base\custom

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %BUILD_DIR%\themes mkdir %BUILD_DIR%\themes
if not exist %BUILD_DIR%\fonts mkdir %BUILD_DIR%\fonts
if not exist %BUILD_DIR%\lexer_gen mkdir %BUILD_DIR%\lexer_gen

pushd %BUILD_DIR%
REM call %CUSTOM_DIR%\bin\build_one_time %CUSTOM_DIR%\languages\4coder_cpp_lexer_gen.cpp %BUILD_DIR%\lexer_gen
REM %BUILD_DIR%\lexer_gen\one_time.exe

call %CUSTOM_DIR%\bin\buildsuper_x64-win.bat %ROOT%\custom.cpp debug
xcopy /Y /F %ROOT%\themes "%BUILD_DIR%\themes"
xcopy /Y /F %ROOT%\fonts "%BUILD_DIR%\fonts"

xcopy /Y /F %ROOT%\config.4coder "%BUILD_DIR%\"
xcopy /Y /F %ROOT%\bindings.4coder "%BUILD_DIR%\"

xcopy /Y /F %ROOT%\4coder_base\fonts "%BUILD_DIR%\fonts"
xcopy /Y /F %ROOT%\4coder_base\4ed.exe "%BUILD_DIR%\"
xcopy /Y /F %ROOT%\4coder_base\4ed_app.dll "%BUILD_DIR%\"
popd

