@echo off
rem Builds old\render.exe from a previous revision's sources placed in old\
rem (eg.h eg.cpp pd.h pd.cpp voice.h voice.cpp const.h), e.g.
rem   for f in eg.h eg.cpp pd.h pd.cpp voice.h voice.cpp const.h; do git show e9b7dbc:$f > analysis/harness/old/$f; done
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0old
cl /nologo /O2 /EHsc /std:c++17 /I. /I..\..\..\..\Libs\VST_SDK\vst3sdk ..\render.cpp voice.cpp pd.cpp eg.cpp /Fe:render.exe
