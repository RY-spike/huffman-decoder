# 伊力扎提：字节编码模块中期交付说明

整理日期：2026-10-24（Asia/Shanghai）。依据《霍夫曼项目_中期答辩三人任务与知识清单》2026-10-23 版第五、七节，核对当前 `feature/encoder-integration` 工作区及已同步的 `origin/main`（`8a15d7a9260b9f7e405dbd2207663b5c70e4ef3b`）。本材料对应已拆除 hybrid 接入的正式字节编码与三种解码器，证据与测试见 [本轮复核记录](../tests/records/2026-10-24-encoder-midterm-synced.md)。早期本地实验与同步前记录另留本机，不是阅读或构建本材料的依赖。

## 已集成实现与处理流程

入口是 `include/huffman/encoder.hpp` 的 `encode_bytes(input)`，实现位于 `src/encoder.cpp`；来源及接入方式见 [encoder-integration.md](encoder-integration.md)。输入是原始字节数组，符号编号为 0～255；UTF-8 汉字可能占多个字节，当前不是 Unicode 字符级或词级编码。

```text
原始字节
  → 统计频率（256 个计数器）
  → 建树（反复合并频率最低的两个节点）
  → 取码长（叶节点深度；单符号强制为 1）
  → 分配规范码（按码长升序，再按符号编号升序）
  → 写位流（码字高位先写，末字节低位补零）
  → lengths + data → 建解码表 → 解码并与原始字节逐项比较
```

低频符号允许较深路径，从而降低加权总码长。相同频率按子树最小符号编号打破平局，保证本实现可重复。树只提供码长；规范码按固定顺序重新分配，双方用相同码长和排序规则即可得到同一码表。首个码为 0；后续码为“前一码加 1，再按码长增量左移”。前缀码使完整码字不成为另一码字的前缀，从有效位起点可以逐个识别符号。

空输入输出空码表和空位流。单符号使用 1 位码 `0`。树生成码长超过 32 位时抛出 `std::invalid_argument`，不会截断，也未实现限长算法；位数溢出会报错。

## ABCA 手算表

输入共 4 字节。先合并 B、C（各 1 次），再与 A（2 次）合并；得到 A 深度 1，B、C 深度 2。

| 符号 | 字节编号 | 频率 | 码长 | 规范码 |
|---|---:|---:|---:|---|
| A | 65 | 2 | 1 | `0` |
| B | 66 | 1 | 2 | `10` |
| C | 67 | 1 | 2 | `11` |

`ABCA = 0 | 10 | 11 | 0 = 010110`。有效位数为 `2×1 + 1×2 + 1×2 = 6`。按 MSB-first 存储，从字节的 bit 7 向 bit 0 写入；末尾低 2 位补零：`01011000 = 0x58`。因此 `bytes={0x58}`、`bit_count=6`、`symbol_count=4`。最后两位是存储填充，不能再解成 A。

## 接口四项关系与结束条件

| 内容 | 含义 | ABCA |
|---|---|---|
| `vector<SymbolLength> lengths` | 每项给出 `symbol_id`、`bit_length`；描述如何重建规范码，不保存原文顺序 | `(65,1),(66,2),(67,2)` |
| `EncodedData.bytes` | 拼接码字后存储的位流字节 | `58`（十六进制） |
| `EncodedData.bit_count` | 有效位数，不包含末字节补零 | 6 |
| `EncodedData.symbol_count` | 应还原的原始符号数，字节模式等于原文字节数 | 4 |

本编码器满足 `bit_count = Σ(频率×码长)`、`bytes.size() = ceil(bit_count/8)`、`symbol_count = input.size()`。解码只消费有效位，完成后输出符号数必须匹配；只知道存储字节数无法判断补零边界，只知道符号数也不足以检验有效位流是否合法。`lengths` 和三个数据字段须一起交接。`data` 是内存负载，旧 HUF1 文件头不能直接传给解码器。完整约定见 [interface.md](interface.md)。

## 特殊输入与正确性

已与当前 `include/huffman/types.hpp`、`encoder.hpp`、`src/encoder.cpp`、[interface.md](interface.md)及现有测试核对，上述 ABCA、MSB-first、有效位与符号计数约定一致。现有 `tests/test_encoder_integration.cpp` 已覆盖空输入、9 个 `0xFF`、ABCA、0～255 各一次、长度 1～65 的循环短输入、65536 字节确定性数据及大于 12 位码长的偏斜输入。检查 baseline/base/buffer 的输出逐项等于原始字节，并检查符号数、负载大小和低位补零。`tests/test_decoder.cpp` 另覆盖跨字节、批次边界、32 位码字和非法输入，并保留本次分离出的通用改进：空码表对应空流、31 位截断长码、非法前缀、符号数过多/过少、空表对应非空流、零有效位却有输出符号，以及 32 位码和短码交替输入。当前正式测试为 4 项，不注册 `hybrid_alice`，不依赖本地实验源码或 Alice 正文。分离前 5/5 为历史状态，分离后的 4/4 结果见复核记录；可选旧 HUF1 检查未作为本次已执行项目。

## 语料统计与证据来源

本节采用已随 `origin/main` 同步的团队正式记录，不重新下载、替换语料或猜测字段。[data/sources.json](../data/sources.json)固定来源与 SHA-256，[datasets.csv](../results/2026-10-23-byte-text/datasets.csv)给出统计。以下语料路径为正式实验使用路径；当前本机没有 `data/corpus/` 正文，不能把历史核验写成本次 Pride 实跑。本机另有与正式 Alice 来源哈希一致的 `tests/data/alice.txt`，仅为本地实验保留，不纳入本次交付，也不是正式测试依赖。

| 字段 | Alice’s Adventures in Wonderland | Pride and Prejudice |
|---|---|---|
| 来源 | Project Gutenberg #11；`sources.json`：`https://www.gutenberg.org/cache/epub/11/pg11.txt` | Project Gutenberg #1342；`sources.json`：`https://www.gutenberg.org/cache/epub/1342/pg1342.txt` |
| 正式实验文件路径 | `data/corpus/alice_gutenberg11.txt` | `data/corpus/pride_gutenberg1342.txt` |
| 原始字节数 / symbol_count | 174311 / 174311 | 772386 / 772386 |
| 不同字节数 | 95 | 104 |
| 最大码长 | 18 bit | 20 bit |
| 有效位数 / bit_count | 834792 bit | 3572748 bit |
| 位流负载字节数 | 104349 | 446594 |
| 末尾补零位数（负载字节数×8−有效位数） | 0 | 4 |
| 平均码长（CSV 精度） | 4.789095353 bit/符号 | 4.625599118 bit/符号 |
| 负载占原文比例（CSV 值×100%） | 59.8636919% | 57.8200537% |
| SHA-256 | `01b38ea4c710a84bc18d0bd41271a5a1a92b94e97b2812f4dece97d4a694725e` | `3f6bb9d6f78e0293b56acd4714dd68cb7d6d1d293402031ce9d5a216bcaf9d75` |

两份文本均使用完整原始字节，保留头尾说明、UTF-8 字节与换行，见 [data/README.md](../data/README.md)。补零位数直接由 CSV 中整数计算，symbol_count 在本字节模式下等于 input_bytes。压缩负载仅计 `bytes`，不包含码表、计数字段、文件头或字典，不等于完整压缩文件比例，也不是解码优化带来的压缩率变化。

### 正式 benchmark 与验证索引

正式记录目录为 [results/2026-10-23-byte-text/](../results/2026-10-23-byte-text/)，数据集编号 1=Alice、2=Pride。可用于报告/PPT的路径如下：

| 文件 | 引用内容 |
|---|---|
| [data/sources.json](../data/sources.json) | 两份语料来源 URL、文件名与固定 SHA-256 |
| [datasets.csv](../results/2026-10-23-byte-text/datasets.csv) | 本节全部输入/位流统计，2 行数据 |
| [measurements.csv](../results/2026-10-23-byte-text/measurements.csv) | 360 条正式计时原始记录，2 文本×3 版本×2 scope×30 次 |
| [summary.csv](../results/2026-10-23-byte-text/summary.csv) | 12 行汇总，含中位数、P25/P75、吞吐量和同 scope 加速比 |
| [verification.csv](../results/2026-10-23-byte-text/verification.csv) | 12 种情况均 PASS，各 33 次完整比较（3 次预热+30 次测量） |
| [file-checksums.csv](../results/2026-10-23-byte-text/file-checksums.csv) | 两文本各三份还原文件的哈希，6 项 matches_input 均为 True |
| [metadata.json](../results/2026-10-23-byte-text/metadata.json) | 实验机器、基准提交、源码/程序哈希、参数、输入与还原文件哈希 |
| [run-info.txt](../results/2026-10-23-byte-text/run-info.txt)、[ctest.log](../results/2026-10-23-byte-text/ctest.log)、[status.txt](../results/2026-10-23-byte-text/status.txt) | 编译器与配置、当时标准检查 4/4 通过、实验成功状态 |
| [benchmarking.md](benchmarking.md)、[byte-benchmark-results.md](byte-benchmark-results.md) | 复现方法、计时边界、结果解释与限制 |

正式结果为 2026-10-23 在 i9-12900H、MSVC 19.44.35229、x64 Release 上采集；`metadata.json` 记录基于 `22eaa739c59b7f0c6c8adcf949d6f695ff806e45` 加当时工作区修改，并保存实际文件哈希。当前同步提交包含这些历史结果，不表示当前本地混合表工作区就是当时计时的源码版本。

| 文本 | 纯解码 baseline 中位 ms | 纯解码 buffer 中位 ms | 纯解码 buffer 加速比 |
|---|---:|---:|---:|
| Alice | 2.127100000 | 1.400800000 | 1.518489435 |
| Pride | 9.115350000 | 6.053100000 | 1.505897804 |

此处 `scope=decode_symbols`，包含解码及预分配输出缓冲区的清空/填写，不含读文件、编码、建表、编号到字节还原、比较和写文件。`decode_and_restore_bytes` 另包含编号到字节转换，不能与前者交叉计算加速比。正式比较仅 baseline/base/buffer，未测混合表；本地初步混合表结果也不能与这组正式耗时拼接。以上是特定环境和两份输入上的结果，不推广为所有数据均快 1.5 倍。

## 个人贡献与后续计划（供本人核对后用于报告）

伊力扎提·阿布力克木负责字节编码、规范码与码表说明。已提供字节频率统计、霍夫曼树与码长生成、规范码分配和位流写入实现，按团队 `SymbolLength`/`EncodedData` 接口完成编码接入及联调检查；完成本地 Alice 字节语料实验记录，为团队两份真实文本的正式字符级（按字节符号）实验提供编码输入，并核对、整理正式来源、统计和验证证据。本次补齐手算表、位序与结束条件、特殊输入及语料说明。团队正式 benchmark 的采集与解码优化由团队分工完成，本人未在本机重新执行 Pride 全文实验；不能将团队所有计时归为个人独立完成。工具辅助、本人理解与实际贡献应按学校要求如实确认。

中期按最新版清单收口，不要求继续实现词级或混合表。词级属于后续：需保留空格、标点与原文顺序，设计可逆切分、词/片段编号及反向字典，并将字典开销纳入比较。混合表已有本地初版，正确性通过，但目前未形成正加速，属于后续优化。此前本地 p=8/10/12 的初测均慢于同轮基线；本次正式材料不提交该实验代码或性能记录，不将其列为中期核心成果。实验快照及原始记录保留本机，后续可继续研究紧凑表项、输出与加载开销，并在统一环境复测；慢因目前是待验证假设。

因此对 10 月 23 日清单的状态更新应表述为：“混合表不作为中期必交加速成果；本地后来已有功能初版及初步负结果”，不能说完全未实现，也不能宣称已经加速。AVX2、硬件计数器分析保留后续阶段。

## 中期交接状态与剩余事项

编码材料最小交付项已有文件承载：实现及来源说明、ABCA 表、位序和边界说明、测试记录、两份语料统计、流程、贡献与后续计划。混合表仅属于后续优化，不列为中期核心成果。

仍需本人/团队完成：将本说明的编码部分交入学校中期报告和 PPT 并审核；本人确认贡献与实际投入、能口述 ABCA/规范码/结束条件及限制；准备答辩演示与备用截图/备份并参加模拟。当前本机缺少正式路径下两份语料及六份正式还原文件，若本机承担离线双语料演示，应准备与 sources.json 哈希一致的原文并另做现场验证；本轮未下载或替换文件。报告、PPT、导师审核/签字及正式提交未在本次文件整理中完成，不能据此宣布全组中期材料全部完成。
