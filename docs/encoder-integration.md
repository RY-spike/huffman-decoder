# 字节编码器最小集成

## 现有结构与接入规划

- `include/huffman/types.hpp`：共用 `SymbolLength` 与 `EncodedData`。
- `include/huffman/decoder.hpp`：建解码表与三个解码入口。
- `include/huffman/benchmark.hpp`：纯解码计时入口。
- `tests/test_decoder.cpp`：独立可执行测试，使用异常报告失败，无外部测试框架。
- `docs/interface.md`：规范码排序、MSB-first 位序、计数与计时约定。
- 原 CMake 使用 C++17，将解码与计时代码编入 `huffman_decoder`，通过 CTest 注册 `decoder_correctness`。

本次新增 `include/huffman/encoder.hpp`、`src/encoder.cpp`、
`tests/test_encoder_integration.cpp`。CMake 新增独立的 `huffman_encoder` 库，
联调程序同时链接两个库并注册 `encoder_integration`。原解码库无需依赖编码库。

## 接口与来源

`encode_bytes(const std::vector<std::uint8_t>&)` 返回 `EncodeResult`，其中
`lengths` 为按符号编号排序的非零码长列表，`data` 为内存位流。
字节值直接作为符号编号，无额外字典；输入可包含零字节与全部 256 种值。

实现提取自相邻 `huffman_elzat/main.cpp` 的字节编码算法：频率相同时按子树
最小符号编号排序，规范码按 `(码长, 符号编号)` 分配。单符号使用一码位 `0`，
空输入返回空码表与空位流。超过 32 位的树抛出 `std::invalid_argument`，
不截断码字，也没有实现限长 Huffman。位数计数溢出抛出 `std::overflow_error`。

本次不接入 CLI、HUF1 文件读写、TSV 导出、词级切分、旧版解码器副本或生成文件。
HUF1 头不能传给 `decode_*`；解码器仅接收 `result.data`。

```cpp
const auto result = huffman::encode_bytes(input_bytes);
const auto tables = huffman::make_decode_tables(result.lengths);
std::vector<std::uint32_t> restored;
huffman::decode_buffer(result.data, tables, restored);
```

## 验证

沿用 README 的 CMake 构建和 CTest 命令即可运行全部测试。
新增测试覆盖空输入、单符号、ABCA 固定位流、全字节等频率固定输出、
1～65 个符号的边界输入、65536 字节确定性数据及超过 12 位的偏斜码表，
全部经过三个解码入口逐符号核对，另检查补零与计数。
现有解码测试继续独立验证 32 位码字及非法输入。

可选只读旧产物兼容检查：运行 `test_encoder_integration`，传入原
`huffman_elzat` 目录作为唯一参数。检查 sample、empty、single、allbytes、
boundary 五组原 HUF1 文件的码长、有效位数、符号数与原始负载逐字节一致，
同时通过三个现有解码器验证输入还原。不需要将这些本地产物复制入仓库。
