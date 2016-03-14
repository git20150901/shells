@echo off
if exist s3.exe del s3.exe
cl /W3 /nologo /O2 /Os s3.c client.c server.c parse.c cmd.c spp.c wait.c ws2_32.lib shlwapi.lib advapi32.lib
del *.obj