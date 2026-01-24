@echo off

if "%~1" == "" ( 
  exit /b 1
)

set input=%~1
set last5=%input:~-5%

if "%last5%" == ".frag" (
  set file1=%~1
  set "file2=%~2"
) else (
  set file1=%~2
  set "file2=%~1"
)
 
 
echo #pragma once
echo #include ^<string_view^>
echo:
echo constexpr std::string_view FragmentShader = R"(
cat '%file1%'
echo )";
echo: 
echo constexpr std::string_view VertexShader = R"(
cat "%file2%"
echo )";

 
exit /b 0