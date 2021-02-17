@echo off
SET ROOT=%~dp0

if exist %ROOT%\4coder_fleury (
pushd %ROOT%\4coder_fleury
echo "syncing 4coder_fleury..."
git pull
popd
) else git clone https://github.com/grouse/4coder_fleury.git %ROOT%\4coder_fleury

if exist %ROOT%\4coder_base (
pushd %ROOT%\4coder_base
echo "syncing 4coder_base..."
git pull
popd
) else  git clone https://github.com/grouse/4coder_base.git %ROOT%\4coder_base





pause