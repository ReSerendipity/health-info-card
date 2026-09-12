# tools/gen_health_font.ps1 —— 重新生成健康信息卡的中文子集字体。
#
# 用途：当 health_profile.h 里新增了字符集中没有的汉字时,把缺的字加进
#       tools/health_font_chars.txt,然后运行本脚本,再重新编译固件。
#
# 依赖：
#   - node/npm 与 lv_font_conv(npm install -g lv_font_conv)
#   - 开源字体 Noto Sans CJK SC(OFL 许可)。
#     下载地址: https://github.com/googlefonts/noto-cjk
#     默认从本机以下路径读取,可通过 $env:NOTO_FONT 覆盖:
#       C:\Users\Doro\esp\fonts\NotoSansCJKsc-Regular.otf
#
# 产物：覆盖 main/health_font_16.c 与 main/health_font_20.c。
#       LVGL 字体在编译期嵌入固件(存 Flash,不占 RAM 堆)。

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fontFile = $env:NOTO_FONT
if (-not $fontFile) { $fontFile = "C:\Users\Doro\esp\fonts\NotoSansCJKsc-Regular.otf" }
if (-not (Test-Path $fontFile)) {
    throw "找不到字体文件: $fontFile 。请先下载 Noto Sans CJK SC,或用 NOTO_FONT 指定路径。"
}

$chars = (Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot "health_font_chars.txt")) -replace "`r|`n", ""

lv_font_conv --font $fontFile -r 0x20-0x7E --symbols $chars --size 16 --bpp 4 `
    --format lvgl --output (Join-Path $repoRoot "main\health_font_16.c") `
    --lv-font-name lv_font_health_16

lv_font_conv --font $fontFile -r 0x20-0x7E --symbols $chars --size 20 --bpp 4 `
    --format lvgl --output (Join-Path $repoRoot "main\health_font_20.c") `
    --lv-font-name lv_font_health_20

Write-Output "字体已重新生成: main/health_font_16.c, main/health_font_20.c"
