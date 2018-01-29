IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID

set quetoo_folder=%~1
set build_platform=%~2
set build_configuration=%~3

call ROBO "../mingw-cross/Quetoo-i686/etc/fonts/" "%quetoo_folder%/etc/fonts/" *
call ROBO "../src/client/renderer/shaders/" "%quetoo_folder%/lib/default/shaders/" *

call ROBO "../../Objectively/Objectively.vs15/bin/%build_platform%%build_configuration%/" "%quetoo_folder%/bin/" Objectively.*
call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/bin/%build_platform%%build_configuration%/" "%quetoo_folder%/bin/" ObjectivelyMVC.*

call ROBO "libs/gettext/%build_platform%/bin/" "%quetoo_folder%/bin/" *.dll
call ROBO "libs/glib/%build_platform%/bin/" "%quetoo_folder%/bin/" *.dll

call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll
call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl_image/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll
call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl_ttf/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll

call ROBO "libs/openal/bin/%build_platform%/" "%quetoo_folder%/bin/" *.dll

GOTO DONE

:INVALID
echo "No build input folder. Gonedy."

:DONE