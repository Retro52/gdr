@echo off
rem ===========================================================================
rem  Usage:
rem    format-all.cmd <clang-format> [root-dir] [--style STYLE] [--check]
rem ===========================================================================
setlocal EnableExtensions DisableDelayedExpansion

rem ---- configuration --------------------------------------------------------
set "EXTS=.c .cc .cpp .cxx .c++ .h .hh .hpp .hxx .h++ .inl .ipp .tpp .ixx .cppm .cu .cuh"
set "EXCLUDE_DIRS=.git .github .LICENSES .THIRD_PARTY_NOTICES .svn .hg .vs vendors cmake cmake-build-* bin-*"

rem ---- arguments ------------------------------------------------------------
set "CF="
set "ROOT="
set "STYLE=file"
set "CHECK="

:parse_args
if "%~1"=="" goto :args_done
if "%~1"=="/?" goto :usage
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage
if /i "%~1"=="--check" (
    set "CHECK=1"
    shift
    goto :parse_args
)
if /i "%~1"=="--style" (
    if "%~2"=="" (
        echo Error: --style needs a value.
        exit /b 2
    )
    set "STYLE=%~2"
    shift
    shift
    goto :parse_args
)
if not defined CF (
    set "CF=%~1"
) else if not defined ROOT (
    set "ROOT=%~1"
) else (
    echo Error: unexpected argument "%~1"
    exit /b 2
)
shift
goto :parse_args

:args_done
if not defined CF goto :usage
if not defined ROOT set "ROOT=%CD%"

rem ---- locate clang-format --------------------------------------------------
if not exist "%CF%" goto :cf_search
for %%I in ("%CF%") do set "CF=%%~fI"
goto :cf_check

:cf_search
set "FOUND="
for /f "delims=" %%P in ('where "%CF%" 2^>nul') do if not defined FOUND set "FOUND=%%P"
if not defined FOUND (
    echo Error: clang-format not found: "%CF%"
    exit /b 2
)
set "CF=%FOUND%"

:cf_check
"%CF%" --version >nul 2>&1
if errorlevel 1 (
    echo Error: "%CF%" did not run as clang-format.
    exit /b 2
)

rem ---- normalize root -------------------------------------------------------
if not exist "%ROOT%\" (
    echo Error: directory not found: "%ROOT%"
    exit /b 2
)
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
call :strlen

rem ---- prepare exclude patterns ---------------------------------------------
rem Each entry becomes a search string tested against the file's folder path
rem relative to the root, written as \dir1\dir2\ :
rem   name   -> \name\     *name -> name\     name* -> \name     *name* -> name
rem for /f is used to split the list because a plain "for" would treat
rem entries containing * as file wildcards.
set "EXN=0"
set "REST=%EXCLUDE_DIRS%"

:ex_next
if not defined REST goto :ex_done
set "CUR=%REST%"
set "REST="
set "TOK="
for /f "tokens=1*" %%A in ("%CUR%") do (
    set "TOK=%%A"
    set "REST=%%B"
)
if not defined TOK goto :ex_done
set "S=%TOK%"
if "%S:~0,1%"=="*" (set "S=%S:~1%") else (set "S=\%S%")
if not defined S goto :ex_next
if "%S:~-1%"=="*" (set "S=%S:~0,-1%") else (set "S=%S%\")
if not defined S goto :ex_next
if not "%S:**=%"=="%S%" (
    echo Warning: ignoring exclude pattern "%TOK%" - only a leading or trailing * is supported.
    goto :ex_next
)
set /a EXN+=1
set "EX_%EXN%=%S%"
goto :ex_next

:ex_done

echo clang-format: %CF%
"%CF%" --version
echo Root:         %ROOT%
echo Style:        %STYLE%
echo Excluding:    %EXCLUDE_DIRS%
if defined CHECK (echo Mode:         check only) else (echo Mode:         format in place)
echo.

rem ---- main loop ------------------------------------------------------------
set /a COUNT=0, FAILED=0, SKIPPED=0

for /r "%ROOT%\" %%F in (*) do (
    for %%E in (%EXTS%) do if /i "%%~xF"=="%%E" (
        set "FILE=%%F"
        set "FDIR=%%~dpF"
        call :format
    )
)

echo.
if defined CHECK goto :summary_check
echo Formatted %COUNT% file(s), %FAILED% error(s), %SKIPPED% skipped in excluded folders.
goto :summary_done
:summary_check
echo Checked %COUNT% file(s): %FAILED% need formatting, %SKIPPED% skipped in excluded folders.
:summary_done

if %FAILED% GTR 0 exit /b 1
exit /b 0


:format
setlocal EnableDelayedExpansion
set "REL=!FILE:~%ROOTLEN%!"
set "DD=!FDIR:~%ROOTLEN%!"

for /l %%N in (1,1,%EXN%) do (
    for /f "delims=" %%S in ("!EX_%%N!") do (
        if not "!DD:%%S=!"=="!DD!" (
            endlocal
            set /a SKIPPED+=1
            goto :eof
        )
    )
)

set "RES=0"
if defined CHECK (
    "!CF!" --dry-run -Werror "--style=!STYLE!" "!FILE!" >nul 2>&1 || (
        set "RES=1"
        echo   needs formatting: !REL:~1!
    )
) else (
    echo   !REL:~1!
    "!CF!" -i "--style=!STYLE!" "!FILE!" || (
        set "RES=1"
        echo   ERROR: !REL:~1!
    )
)
endlocal & set /a "COUNT+=1, FAILED+=%RES%"
goto :eof


:strlen
setlocal EnableDelayedExpansion
set "S=!ROOT!#"
set "LEN=0"
for %%P in (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) do (
    if "!S:~%%P,1!" NEQ "" (
        set /a "LEN+=%%P"
        set "S=!S:~%%P!"
    )
)
endlocal & set "ROOTLEN=%LEN%"
goto :eof


:usage
echo Usage: %~nx0 ^<clang-format^> [root-dir] [--style STYLE] [--check]
echo.
echo   ^<clang-format^>   path to clang-format.exe, or a name on PATH
echo   [root-dir]       directory to scan (default: current directory)
echo   --style STYLE    clang-format style (default: file)
echo   --check          report unformatted files without changing them
exit /b 2
