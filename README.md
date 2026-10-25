# 规范霍夫曼解码器加速实验

大学生创新项目：规范霍夫曼解码器实现与加速实验。仓库地址：`https://github.com/RY-spike/huffman-decoder`。

目前已接入字节编码器，以及伊力亚尔负责的三个解码版本：基线、合并 `baseSym` 与 `baseCwd` 的 `Base` 版、64 位批量缓冲版。接口见 [`docs/interface.md`](docs/interface.md)，编码器说明见 [`docs/encoder-integration.md`](docs/encoder-integration.md)。词级编码、混合表及 AVX2 模块待组员交付后接入。

## 构建与检查

中期阶段的收尾材料已集中到以下入口。按最新通知，汇报预计在11月初、具体日期待通知，PPT自述共5分钟。

- [伊力亚尔的交付状态、个人贡献与进度对照](docs/midterm/ilyar-midterm-handoff.md)
- [离线演示操作说明](docs/midterm/demo-guide.md)
- [五分钟自述技术素材](docs/midterm/five-minute-outline.md)
- [离线备用记录页面](docs/midterm/demo-backup.html)（下载到本地后用浏览器打开）

已完成Release编译并备齐语料后，可在仓库根目录执行 `./scripts/run_midterm_demo.ps1`，离线核对ABCA、4项CTest与两份全文。现场演示计时不替代正式性能记录。

在 **Visual Studio 2022 Developer PowerShell** 中使用 CMake（该终端会将 CMake 加入 `PATH`）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

三个解码入口接收同一个 `EncodedData` 和 `DecodeTables`。性能比较时应使用同一份编码输入与码表，先检查输出一致，再计时。`benchmark_decoder` 提供预热、重复测量、中位数、每符号耗时和吞吐量。当前测试覆盖手工位流、跨字节边界、单符号、空输入、截断输入、4096 符号流及 32 位长码字。

## 真实文本实验

在仓库根目录的 PowerShell 中运行：

```powershell
./scripts/fetch_corpus.ps1
./scripts/run_benchmarks.ps1 -OutputDirectory results/my-first-run
```

运行脚本会寻找 Visual Studio 2022 内置的 CMake 并构建 Release 程序，不要求另外安装 Python 或测试框架。每次指定一个新的结果目录。源码与程序已经同步时，可加 `-SkipBuild` 跳过构建。

实验分别测量“输出符号编号”和“再还原为原文字节”两个范围，每个版本预热 3 次、测量 30 次；保存全部测量、统计汇总、输入和输出哈希及编译环境。还原文件位于结果目录下的 `restored/`，不计入计时、不提交到仓库。

- [实验方法与复现步骤](docs/benchmarking.md)
- [2026 年 10 月 23 日字节级实验结果](docs/byte-benchmark-results.md)
- [原始实验记录](results/2026-10-23-byte-text/)
- [语料来源与版本](data/README.md)

这轮实验只支持本机、这两份字节级文本和当前实现的比较；未测试词级、混合表或 AVX2，不与原论文的数据直接比较。

算法背景：Zavadskyi 与 Kovalchuk，*Optimization of the Canonical Huffman Codes for Word-Based Natural Language Text Compression*，2025，DOI: 10.17721/1812-5409.2025/2.25。项目报告应区分已有研究方法、团队复现与团队新增实验。
