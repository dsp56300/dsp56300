set outdir=..\temp\cmake_win32\
cmake . -B %outdir% -G "Visual Studio 15 2017"
IF %ERRORLEVEL% NEQ 0 exit /B 1
pushd %outdir%
cmake --build . --config Release
IF %ERRORLEVEL% NEQ 0 (
	popd 
	exit /B 2
)
