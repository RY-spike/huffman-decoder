# 规范霍夫曼解码器加速实验

大学生创新项目：规范霍夫曼解码器实现与加速实验。仓库地址：`https://github.com/RY-spike/huffman-decoder`。

目前实现了伊力亚尔负责的三个解码版本：基线、合并 `baseSym` 与 `baseCwd` 的 `Base` 版、64 位批量缓冲版，以及统一计时函数。接口见 [`docs/interface.md`](docs/interface.md)。编码器、词级语料、混合表及 AVX2 模块由其他成员接入。

## 构建与检查

在 **Visual Studio 2022 Developer PowerShell** 中使用 CMake（该终端会将 CMake 加入 `PATH`）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

三个解码入口接收同一个 `EncodedData` 和 `DecodeTables`。性能比较时应使用同一份编码输入与码表，先检查输出一致，再计时。`benchmark_decoder` 提供预热、重复测量、中位数、每符号耗时和吞吐量。当前测试覆盖手工位流、跨字节边界、单符号、空输入、截断输入、4096 符号流及 32 位长码字。

算法背景：Zavadskyi 与 Kovalchuk，*Optimization of the Canonical Huffman Codes for Word-Based Natural Language Text Compression*，2025，DOI: 10.17721/1812-5409.2025/2.25。项目报告应区分已有研究方法、团队复现与团队新增实验。
