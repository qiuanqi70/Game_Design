#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MD_DIR="$SCRIPT_DIR/Markdown"
LATEX_DIR="$SCRIPT_DIR/latex"
OUTPUT_DIR="$SCRIPT_DIR/output/pdf"
TMP_DIR="$SCRIPT_DIR/tmp"
TMP_LATEX_DIR="$TMP_DIR/latex-build"

OUTPUT_NAME="AlleyFist_midterm_report"
OUTPUT_TEX="$TMP_LATEX_DIR/$OUTPUT_NAME.tex"
OUTPUT_PDF="$OUTPUT_DIR/$OUTPUT_NAME.pdf"
HEADER_TEX="$LATEX_DIR/report_header.tex"
TITLEPAGE_TEX="$LATEX_DIR/titlepage.tex"
FILTER_LUA="$LATEX_DIR/report_filter.lua"

for tool in pandoc xelatex; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "错误: 未找到 $tool，请先安装后再运行本脚本。" >&2
    exit 1
  fi
done

shopt -s nullglob
INPUT_MDS=("$MD_DIR"/*.md)
if (( ${#INPUT_MDS[@]} == 0 )); then
  echo "错误: 未在 $MD_DIR 中找到 Markdown 文件。" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR" "$TMP_LATEX_DIR"

pandoc "${INPUT_MDS[@]}" \
  --standalone \
  --from markdown+implicit_figures+pipe_tables+tex_math_dollars \
  --to latex \
  --toc \
  --toc-depth=2 \
  --lua-filter="$FILTER_LUA" \
  --include-in-header="$HEADER_TEX" \
  --include-before-body="$TITLEPAGE_TEX" \
  --syntax-highlighting=pygments \
  --metadata=documentclass:ctexart \
  --metadata=classoption:"12pt,fontset=none" \
  --metadata=date:"" \
  --output "$OUTPUT_TEX"

(
  cd "$SCRIPT_DIR"
  export TEXINPUTS="$LATEX_DIR//:"
  xelatex -interaction=nonstopmode -halt-on-error -output-directory="$TMP_LATEX_DIR" "$OUTPUT_TEX" >/dev/null
  xelatex -interaction=nonstopmode -halt-on-error -output-directory="$TMP_LATEX_DIR" "$OUTPUT_TEX" >/dev/null
)

if [[ ! -f "$TMP_LATEX_DIR/$OUTPUT_NAME.pdf" ]]; then
  echo "错误: PDF 生成失败。" >&2
  exit 1
fi

cp "$TMP_LATEX_DIR/$OUTPUT_NAME.pdf" "$OUTPUT_PDF"

rm -rf "$TMP_LATEX_DIR"
rmdir "$TMP_DIR" 2>/dev/null || true

echo "编译成功: $OUTPUT_PDF"
