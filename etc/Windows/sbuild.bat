rem Build the dependencies and then tlRender, into directories beside the
rem current one. What to build is in etc/Config/*.cmake rather than here: this
rem script is only the part that differs between platforms.
rem
rem Every command is followed by an errorlevel check, which is what "set -e"
rem does for the Linux and macOS scripts. Without them a failed stage carries
rem on into the next, so the error that gets reported is the confusion further
rem down rather than the one that mattered.

set SOURCE_DIR=%1
set BUILD_TYPE=%2
set CONFIG=%3
IF "%CONFIG%"=="" set CONFIG=default
set CONFIG_FILE=%SOURCE_DIR%/etc/Config/%CONFIG%.cmake

rem Build with every core unless told otherwise; cmake --build reads this.
IF "%CMAKE_BUILD_PARALLEL_LEVEL%"=="" set CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%

git -C %SOURCE_DIR% submodule update --init --recursive
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake ^
    -S %SOURCE_DIR%/deps/ftk/etc/SuperBuild ^
    -B ftk-%BUILD_TYPE% ^
    -C %CONFIG_FILE% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX=%CD%/install-%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH=%CD%/install-%BUILD_TYPE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
cmake --build ftk-%BUILD_TYPE% --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake ^
    -S %SOURCE_DIR%/etc/SuperBuild ^
    -B tl-%BUILD_TYPE% ^
    -C %CONFIG_FILE% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX=%CD%/install-%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH=%CD%/install-%BUILD_TYPE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
cmake --build tl-%BUILD_TYPE% --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake ^
    -S %SOURCE_DIR% ^
    -B build-%BUILD_TYPE% ^
    -C %CONFIG_FILE% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX=%CD%/install-%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH=%CD%/install-%BUILD_TYPE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
cmake --build build-%BUILD_TYPE% --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

rem The install directory is how everything downstream finds what was built:
rem the tests import ftkPy from there, and packaging reads it.
cmake --build build-%BUILD_TYPE% --config %BUILD_TYPE% --target install
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
