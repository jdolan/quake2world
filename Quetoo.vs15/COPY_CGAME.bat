IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID

set quetoo_folder=%1
set build_name=%2

copy "bin\%build_name%\cgame*" "%quetoo_folder%\lib\default\*" /y

GOTO DONE

:INVALID
echo "No build input folder. Gonedy."

:DONE