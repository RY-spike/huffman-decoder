# 真实文本验证与计时方法

## 当前覆盖范围

用已接入的 `encode_bytes` 编码整个文件，再用 `decode_baseline`、`decode_base`、`decode_buffer` 解码。一个符号等于一个字节；UTF-8 文本中的多字节字符按原始字节处理。此阶段不包含词级编码、混合表或 AVX2。

## 复现步骤

在仓库根目录打开 PowerShell，执行：

```powershell
./scripts/fetch_corpus.ps1
./scripts/run_benchmarks.ps1 -OutputDirectory results/my-run
```

依赖为 Git、Visual Studio 2022 的 C++ 工作负载、其内置 CMake 和 PowerShell。首次下载语料需要联网。输出目录必须尚不存在，防止覆盖旧实验。脚本会检查数据哈希、构建 x64 Release、运行实验并记录环境。标准正确性测试可通过以下命令运行：

```powershell
# 在能找到 CMake 的 Visual Studio Developer PowerShell 中执行
ctest --test-dir build -C Release --output-on-failure
```

已编译时可以直接运行程序：

```powershell
./build/Release/benchmark_files.exe --input data/corpus/alice_gutenberg11.txt --input data/corpus/pride_gutenberg1342.txt --output-dir results/manual-run --warmup 3 --runs 30
```

直接运行 C++ 程序会生成 CSV、还原文件及编译信息。PowerShell 包装脚本还会补充 CPU、电源模式、源文件和程序哈希、独立的还原文件哈希检查等元数据，因此正式实验优先使用脚本。

## 计时边界

| scope | 包含 | 不包含 |
| --- | --- | --- |
| decode_symbols | 解码核心、原有输入检查、清空并填写预分配的符号输出缓冲区 | 文件读取、编码、码表构建、输出容量预分配、结果核对、文本还原、文件写入 |
| decode_and_restore_bytes | 上一行的过程，再将编号序列转换到预分配的原文字节缓冲区 | 文件读取、编码、码表构建、容量预分配、结果核对、文件写入 |

两种 scope 分别与各自的 baseline 比较。不要把第二种 scope 当成含磁盘 I/O 的完整解压程序耗时。缓冲区的清空和写入仍在计时范围内，纯解码一词不代表仅统计 CPU 算术指令。

当前有 3 个版本 × 2 个 scope 共 6 种情况。每种情况预热 3 次，正式测量 30 次；每轮轮换第一个执行的情况，30 轮恰好轮转 5 遍，减少固定顺序的影响。每次调用后都比较完整编号序列；文本还原 scope 还逐字节比较原文。比较过程在计时区间之外。输入、码表、输出容量在测量前准备好，使用 `steady_clock`。

当前操作系统自行调度线程，未绑定性能核或固定频率。记录了均衡电源模式，后台负载和动态频率仍会影响数据。小幅差异应结合四分位区间和独立复测判断，不据单次比例宣称稳定改进。

## 输出文件

| 文件 | 含义 |
| --- | --- |
| datasets.csv | 输入长度、符号数、最大码长、有效位数、压缩负载大小 |
| measurements.csv | 每次正式测量的原始毫秒数，附轮次和该轮执行顺序 |
| summary.csv | 中位数、P25、P75、最小值、最大值、ns/符号、吞吐量和加速比 |
| verification.csv | 每个版本每种 scope 的完整比较结果和校验次数 |
| file-checksums.csv | 三份实际还原文件的 SHA-256 与原文相等的独立检查结果 |
| metadata.json | CPU、系统、电源模式、命令参数、程序与源文件哈希、输入来源、Git 基准版本 |
| run-info.txt | 编译器、构建配置、计时方式 |
| status.txt | 末行 PASS 表示程序成功；中断或失败的结果不能作为完成的实验 |
| console.log | 运行输出 |
| restored/ | 每份输入按三个版本保存的还原文件，供本地核对，Git 已忽略 |

原始 CSV 按情况分组保存；`round` 和 `order` 可以还原测量顺序。分位数采用排序后线性插值。ns/符号由中位耗时除以符号数得到。字节级实验中“符号/秒”和“原文字节/秒”数值相同，MiB/s 使用 1048576 字节。加速比为同一 scope 的基线中位耗时除以当前中位耗时。

`payload_ratio` 是压缩位流字节数除以原文长度，不包括码表或文件头；不能将它作为完整压缩文件的压缩率。

## 其他成员如何接入

伊力扎提的字节编码器已完成这一轮对接。下一步请提供词级编号、码长、位流和能精确还原空格标点的反向字典，并说明词的切分规则。词级结果需另设实验，不能混入本轮字节级统计。

赛尔达尔可从 `summary.csv` 制作对比图，图中注明 scope、语料、单位和测试环境。原始数据见 `measurements.csv`。独立参考解码器仍可用于加强验证；当前往返测试直接以原始字节作为预期结果。

混合表和 AVX2 模块接入时，先通过同一套完整比较，再在 `benchmark_files.cpp` 的候选列表中注册其函数。与某份输入相关、每次调用都必须执行的准备步骤要计入对应耗时，不能单独移出计时区间后与现有完整调用比较。
