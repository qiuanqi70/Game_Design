# View Assets

`code/view/CMakeLists.txt` 中 `qt_add_resources(... FILES ...)` 的显式清单是游戏和 View 测试实际打包的运行时 QRC 资源。

`Aseprite/`、`PSD/`、逐帧拆图等未登记目录是创作源素材，仅随仓库保存，不会自动进入可执行文件。新增或替换运行时图片、字体时，需要同时把文件登记到该 `FILES` 清单，并保持 `:/art/...` 路径与代码引用一致。
