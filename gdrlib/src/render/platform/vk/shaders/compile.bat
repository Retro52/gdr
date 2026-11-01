@echo off
setlocal

call :compile imgui_blit.vert vs kVK_ImguiBlitVertShader        imgui_blit_vert || goto :fail

call :compile imgui_blit.frag ps kVK_ImguiBlitFrag2DShader      imgui_blit_frag_2d       "-D FOR_SAMPLER_2D" || goto :fail
call :compile imgui_blit.frag ps kVK_ImguiBlitFrag2DArrayShader imgui_blit_frag_2d_array "-D FOR_SAMPLER_2D_ARRAY" || goto :fail

echo All shaders compiled.
exit /b 0

:fail
echo Shader compilation failed.
exit /b 1

:: ---------------------------------------------------------------------------
:: :compile <name> <stage> <array> ["<defines>"]
::   %1  shader name, (imgui_blit.frag)
::   %2  shader stage: ps, vs, ...
::   %3  name of the output array
::   %4  name of the output file
::   %5  optional defines, quoted as one argument
:: ---------------------------------------------------------------------------
:compile
echo Compiling %~1.hlsl ...
dxc.exe ^
    -spirv -fspv-target-env=vulkan1.3  ^
    -T %~2_6_6 -E main -WX -O3         ^
    -Vn %~3                            ^
    -Fh vk_%~4.hpp                     ^
    -fvk-use-dx-layout                 ^
    -fvk-support-nonzero-base-vertex   ^
    -fvk-support-nonzero-base-instance ^
    %~5                                ^
    %~1.hlsl

exit /b %errorlevel%
