# 三人共用接口

## 编码与位流约定

- `symbol_id` 是 `uint32_t`。字符级试验用原文字节值 0～255；词级试验由编码组定义稳定的编号和反向字典。解码器只输出编号，全文还原另行计时。
- 编码组传入每个符号的码长 `SymbolLength`。解码器按 `(bit_length, symbol_id)` 升序生成规范码。有效码长为 1～32；空码表仅用于空输入。单符号码表必须使用码长 1，码字为 `0`。
- 每个码字从最高有效位开始写入。字节内部也从最高位向最低位写入。最后一个字节不足 8 位时低位补零，`bit_count` 只记录有效位。
- `symbol_count` 是原始符号个数。成功解码必须刚好消费 `bit_count` 位并输出 `symbol_count` 个编号。
- `EncodedData` 与码长列表均为内存对象。文件格式和词级切分由编码组随后确定，不影响解码核心接口。

## 三人交接

伊力扎提：提供 `std::vector<SymbolLength>`、`EncodedData`、符号编号到原始文本片段的字典，以及小样例的预期结果。编码器必须使用上述规范码和位序。

伊力亚尔：调用 `make_decode_tables` 一次，分别调用 `decode_baseline`、`decode_base`、`decode_buffer`。输出是符号编号序列。

赛尔达尔：针对同一份输入比较三种输出与独立参考结果。AVX2 版本可沿用相同函数签名。

## 计时边界

纯解码耗时从调用 `decode_*` 开始，到编号输出完成为止；不包括文件读取、码表构建、编码和编号到文本的转换。由于当前接口会清空并扩展 `output`，该耗时包含输出缓冲区操作。所有版本使用相同输入、码表、编译配置和重复次数。全文还原耗时应单独记录。

单个解码器的计时入口为 `benchmark_decoder`，见 `include/huffman/benchmark.hpp`。返回的 `samples_ms` 保留实际采集顺序，`summarize_timings` 计算中位数、四分位数和吞吐量。

多版本、真实文件的统一入口为 `benchmark_files`。它轮换各版本的测量顺序，分别记录 `decode_symbols` 和 `decode_and_restore_bytes`，预分配输出空间并在每次计时结束后核对结果。后者包括从编号还原字节，不包括磁盘读写。方法见 `docs/benchmarking.md`；首轮字节级实测记录见 `docs/byte-benchmark-results.md`。

## 手工样例

`A=0`、`B=10`、`C=11` 时，`ABCA` 的有效位是 `010110`。`bytes={0x58}`、`bit_count=6`、`symbol_count=4`；低两位是补齐位，不能解成额外的 `A`。
