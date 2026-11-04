$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$external = Join-Path $root "external"
New-Item -ItemType Directory -Force -Path $external | Out-Null

if (-not (Test-Path (Join-Path $external "JUCE/CMakeLists.txt"))) {
    git clone --depth 1 https://github.com/juce-framework/JUCE.git (Join-Path $external "JUCE")
}
if (-not (Test-Path (Join-Path $external "NeuralAmpModelerCore/NAM/get_dsp.cpp"))) {
    git clone --depth 1 --recurse-submodules https://github.com/sdatkinson/NeuralAmpModelerCore.git (Join-Path $external "NeuralAmpModelerCore")
}
Write-Host "Dependencies cloned. Configure with:"
Write-Host "  cmake -S . -B build -G 'Visual Studio 17 2022' -A x64"
Write-Host "Then build with:"
Write-Host "  cmake --build build --config Release"
