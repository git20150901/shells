@echo off
if exist s4.exe del s4.exe
cl /W3 /nologo /O2 /Os s4.c client.c server.c parse.c cmd.c spp.c wait.c ws2_32.lib shlwapi.lib advapi32.lib
del *.obj