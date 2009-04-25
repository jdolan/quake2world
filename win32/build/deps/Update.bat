@echo off
echo This program will update Quake2World.
echo Please be patient, this may take a while.
rsync.exe -rz --progress --exclude=rsync.exe --exclude=cygwin1.dll rsync://jdolan.dyndns.org/quake2world-win32 .
rsync.exe --delete -rz --progress rsync://jdolan.dyndns.org/quake2world temp
xcopy temp\default default /E /Y >nul
echo Update complete!
echo Make sure to run this file on a regular basis!
pause
