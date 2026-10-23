# 文本语料

实验使用 Project Gutenberg 的两份英文纯文本：

| 文件 | 作品 | 来源 |
| --- | --- | --- |
| alice_gutenberg11.txt | Alice's Adventures in Wonderland | https://www.gutenberg.org/ebooks/11 |
| pride_gutenberg1342.txt | Pride and Prejudice | https://www.gutenberg.org/ebooks/1342 |

2026 年 10 月 23 日下载时分别为 174311 和 772386 字节。下载地址与 SHA-256 固定在 `sources.json`。程序使用整个原始文件，包括 UTF-8 字节、换行及书前书后说明；不剥离内容，不规范化换行。这里的 Alice 不是 Canterbury 的 alice29.txt，不能直接与论文表中的 alice29 耗时比较。

在仓库根目录运行 `./scripts/fetch_corpus.ps1` 可下载到 `data/corpus/`。文件保留来源中的说明和许可文本。语料正文与还原文件不随代码提交；组员按脚本下载并核对哈希。若来源更新导致哈希不同，应作为另一份语料版本记录，不能沿用旧实验的结果。
