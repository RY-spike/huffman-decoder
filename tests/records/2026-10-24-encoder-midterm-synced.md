# 同步正式结果后的编码材料复核

日期：2026-10-24（Asia/Shanghai）。分支：feature/encoder-integration。
本轮 HEAD 与 origin/main 均为 8a15d7a9260b9f7e405dbd2207663b5c70e4ef3b，未生成新提交。本文保留同步时包含 hybrid 的历史验证，末节给出剥离 hybrid 后的正式交付状态：构建成功、CTest 4/4。
本记录更新同步前“Pride 数据缺失、仅 3 项检查”的状态；早期本地记录继续保留，但不是正式交付依赖。
完整命令、数据核对与分离前后测试输出见 [本轮日志](2026-10-24-encoder-midterm-synced.log)。

## 已同步的正式证据

- data/sources.json 与 results/2026-10-23-byte-text/ 已包含在 origin/main；本轮确认这些文件与该引用一致。
- datasets.csv 共 2 行：Alice 174311 字节、95 种、最大码长 18、834792 有效位、104349 负载字节；Pride 772386 字节、104 种、最大码长 20、3572748 有效位、446594 负载字节。
- 由整数计算补零位：Alice 0、Pride 4；由 input_bytes 对应字节模式 symbol_count。平均码长与负载比例核对 CSV 舍入精度后相符。
- measurements.csv 360 行，summary.csv 12 行，每种情况 30 条正式测量、3 次预热；从原始耗时重算全部 12 个中位数，与 summary.csv 相符。
- verification.csv 12 行，均 PASS、各 33 次核验；file-checksums.csv 6 行，均 matches_input=True；哈希与 sources.json 和 metadata.json 输入记录一致。status.txt 末行为 PASS。
- 上述为历史记录的一致性复核，不是本机重做两份文本的 360 条正式实验，也不是重新散列六份历史还原文件。metadata.json 中的实验机器是 i9-12900H，run-info.txt 为 MSVC 194435229、Release；当时标准检查日志是 4/4。

## 接口与特殊输入核对

已逐项核对 include/huffman/types.hpp、include/huffman/encoder.hpp、src/encoder.cpp、docs/interface.md、tests/test_encoder_integration.cpp 和 tests/test_decoder.cpp：

- ABCA：A 频率 2、B/C 各 1；规范码 A=0、B=10、C=11；010110 低位补零成 0x58，有效位 6、原始符号数 4。
- 字节内部及码字均先写高位，bit_count 不计低位补零；成功解码消费位数与输出符号数均须匹配。
- 空输入、9 个 0xFF、全 256 字节、1～65 字节短输入、跨字节/批次、确定性数据和大于 12 位码长的既有测试均由现有程序覆盖。非法输入与 32 位码字仍有独立解码断言。
- 单符号采用 1 位码 0；编码树码长超过 32 位会报错，来自源码核对，本轮未另构造超限编码样例。

## 分离前本机测试结果与范围（历史）

Windows / i9-14900HX，MinGW-w64 GCC 15.2.0，C++17，Ninja，Release。
CMake 配置与构建成功；CTest 5/5 通过：decoder_correctness、encoder_integration、benchmark_statistics、benchmark_cli、hybrid_alice。
git diff --check 与 git diff --cached --check 均通过。

本地 tests/data/alice.txt 哈希与正式 sources.json 一致；hybrid_alice 对其执行完整正确性比较，含 p=8/10/12。
benchmark_cli 使用仓库的小型样例，验证 baseline/base/buffer、落盘哈希和结果目录保护；它不替代 Pride 全文测试。
当前 data/corpus/ 两份正文和正式 restored/ 产物均未准备；Pride 本轮只复核正式 CSV/哈希记录，没有下载或重新运行全文。未运行可选旧 HUF1 参数检查，未重测团队正式性能。hybrid_alice 日志中的顺带耗时不升级为正式 benchmark 结论。

## 材料与贡献

更新 docs/encoder-midterm.md：正式双语料统计、来源与验证路径、benchmark 口径及限制、接口复核、个人贡献和待交接事项。
个人贡献限于字节编码器、规范码、接口联调、本地 Alice 实验及对团队正式字节级实验的编码输入与材料整理；不把团队采集的全部计时归为个人独立完成。
混合表表述为“已有本地初版，正确性通过，但目前未形成正加速，属于后续优化”，不列作中期核心成果。词级和 AVX2 仍是后续工作。

剩余中期工作：本人核对贡献并能手讲原理，将编码材料并入正式报告/PPT，准备演示和备份、参加模拟；导师审核签字及正式提交按组内安排。若本机演示两份语料，需另准备固定哈希的原文并现场复核，当前不能声明双语料离线演示就绪。

上述同步核对阶段未暂存新文件，未改 main、commit/push 或改动 stash。正式历史结果均保留。

## 剥离 hybrid 后的正式交付复核（最新）

检查五个 tracked 文件相对 origin/main 的差异，判断如下：

| 文件 | 处理与理由 |
|---|---|
| CMakeLists.txt | 移除 decoder_hybrid.cpp、hybrid_experiment 和 hybrid_alice 接入；保留主线 benchmark/encoder/三种解码器及 4 项测试；保留 MINGW 条件下 -municode，解决正式 benchmark 的 wmain 入口链接问题 |
| docs/interface.md | 新增内容全为 hybrid 接口，已恢复 origin/main 原文 |
| src/decoder_baseline.cpp | 共享内部头的抽取仅为 hybrid 回退服务，已恢复 origin/main 原文，正式库不再依赖 decoder_internal.hpp |
| tests/test_encoder_integration.cpp | 新增 include 和 p=8/10/12 往返循环全为 hybrid，已恢复 origin/main；正式编码边界断言原已存在，继续保留 |
| tests/test_decoder.cpp | 移除 hybrid include、码长重建、调用、查表/回退/p 范围断言；保留与 hybrid 无关的边界用例，见下文 |

保留的通用测试改进：空流+空码表、31 位截断长码、单符号非法前缀、符号数过多与过少、非空流+空码表、零有效位+非零符号数，以及短码/32 位长码交替。最后一项从原 p 循环中抽出，仅执行一次，预期序列为 A、B、16 个 A；所有用例只调用 baseline/base/buffer，不依赖实验库。

在新的 build/formal-midterm 目录配置 Ninja / Release 并完整构建，避免沿用 hybrid 目标或产物。CTest 4/4 通过：decoder_correctness、encoder_integration、benchmark_statistics、benchmark_cli。4 项仍包括编码特殊输入与独立非法输入断言，数量减少是移除了 hybrid_alice，不是删掉主线功能。构建图无 hybrid、decoder_internal.hpp、experiments 或 tests/data/alice.txt 依赖；正式待提交文件也无实验 include/调用。文档链接已核对，不要求提交早期本地记录。

恢复前五个 tracked 文件的完整版本已保存到本地 experiments/hybrid-workspace-backup-20261024/，连同原实验源码和记录保留，未覆盖 stash。该目录不属于正式构建或提交范围。正式 origin/main 本来已有的语料来源、CSV、测试程序和基线实现均未改动。

建议本次定向提交 5 个文件：

- docs/encoder-midterm.md：中期编码说明与双语料正式统计。
- tests/records/2026-10-24-encoder-midterm-synced.md：数据复核及本次分离判断、验证记录。
- tests/records/2026-10-24-encoder-midterm-synced.log：可核对的历史证据和本次正式 4/4 原始日志。
- CMakeLists.txt：仅正式 benchmark 在 MinGW 上的必要链接修复，不含实验接入。
- tests/test_decoder.cpp：只含通用解码边界测试改进。

因此这次可作为“中期材料与验收”提交，包含一项构建兼容修复和通用测试补强；它不是纯 Markdown 提交，也不包含 hybrid 功能。没有新增依赖 experiments、hybrid 源码、decoder_internal.hpp 或本地 Alice 的正式文件。

只留本地：experiments/（含接入快照）、include/huffman/decoder_hybrid.hpp、src/decoder_hybrid.cpp、src/decoder_internal.hpp、tests/data/alice.txt、tests/records/2026-10-24-encoder-midterm.md/.log，以及 .idea/。.idea/ 已加入本仓库 .git/info/exclude，本地忽略不修改共享 .gitignore。

本次不暂存、不 commit/push、不修改 main 或 stash；当前 index 为空，后续只按上述 5 个路径定向暂存，避免混入实验文件。

常用 build 目录也已重新配置、构建并通过 CTest 4/4；旧 hybrid_alice 不再注册，之前生成的实验产物仅留本地。
