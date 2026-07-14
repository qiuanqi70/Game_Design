# AlleyFist 实验报告构建说明

本目录用于维护最终实验报告，正文采用 Markdown 分文件编写，构建脚本会按文件名顺序合并并通过 LaTeX 生成 PDF。

## 目录结构

- `Markdown/`：报告正文与三位成员的分报告。
- `latex/`：封面、页眉页脚、字体、表格和代码块等排版配置。
- `output/pdf/`：最终生成的 PDF。
- `tmp/`：构建或渲染检查过程中的临时文件，脚本成功后会自动清理。

## 构建方式

```bash
./build_report.sh
```

生成结果位于：

```text
output/pdf/AlleyFist_midterm_report.pdf
```

## 后续填写

提交前需要补全以下内容：

- `latex/titlepage.tex` 中的小组成员姓名、学号、课程信息。
- 各分报告开头的 `\sectionauthor{姓名}{学号}`。
- `Markdown/05_personal_reflections.md` 中每位同学的个人反思与收获。
