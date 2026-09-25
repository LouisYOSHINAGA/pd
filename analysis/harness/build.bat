@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /EHsc /std:c++17 /I..\.. /I..\..\..\Libs\VST_SDK\vst3sdk render.cpp ..\..\voice.cpp ..\..\pd.cpp ..\..\eg.cpp /Fe:render.exe
