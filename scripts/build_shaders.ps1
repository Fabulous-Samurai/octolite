# scripts/build_shaders.ps1
$ErrorActionPreference = "Stop"

# glslc'yi PATH'te ara (Vulkan SDK veya MinGW)
$glslc = Get-Command glslc -ErrorAction SilentlyContinue
if (-not $glslc) {
    Write-Host "glslc not found. Trying Vulkan SDK path..." -ForegroundColor Yellow
    if (Test-Path "C:\VulkanSDK\*\Bin\glslc.exe") {
        $glslc = Get-ChildItem "C:\VulkanSDK\*\Bin\glslc.exe" | Select-Object -First 1
    } else {
        Write-Error "glslc bulunamadi! Vulkan SDK yukleyin veya PATH'e ekleyin."
    }
}

$shader_dir = "assets/shaders"
if (-not (Test-Path $shader_dir)) {
    New-Item -ItemType Directory -Path $shader_dir | Out-Null
}

$shaders = @(
    @{ src = "integrate.comp";  dst = "integrate.spv" },
    @{ src = "render.vert";     dst = "render.vert.spv" },
    @{ src = "render.frag";     dst = "render.frag.spv" }
)

foreach ($s in $shaders) {
    $src_path = Join-Path $shader_dir $s.src
    $dst_path = Join-Path $shader_dir $s.dst
    if (-not (Test-Path $src_path)) {
        Write-Warning "Kaynak shader yok: $src_path (atlanıyor)"
        continue
    }
    Write-Host "Derleniyor: $($s.src) -> $($s.dst)" -ForegroundColor Cyan
    & $glslc $src_path -o $dst_path
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Shader derlemesi basarisiz: $($s.src)"
    }
}

Write-Host "Shader derleme tamamlandi." -ForegroundColor Green
