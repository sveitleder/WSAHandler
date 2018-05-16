@echo off

set TARGET=x64

mkdir W:\WSAHandler\build
pushd W:\WSAHandler\build

:: to allow startup without console -subsystem:WINDOWS / this does something -entry:WinMainCRTStartup 
cl -Zi -FC -MD -EHsc W:\WSAHandler\src\WSA_Test.cpp Ws2_32.lib -link /machine:%TARGET% /entry:mainCRTStartup
popd