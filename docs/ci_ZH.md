# 持续集成

[English](ci.md)

本仓库将文档验证、改动分类和固件编译拆分，使每个必需检查都具有稳定且清晰的含义。

## 工作流职责

- [`docs.yml`](../.github/workflows/docs.yml) 检查第一方中英文文档、本地链接、产品图片
  和示例清单。
- [`esp-idf.yml`](../.github/workflows/esp-idf.yml) 对完整 Git 改动集进行分类，选择受影响
  的第一方工程，构建对应矩阵、打包可刷写制品，并输出一个聚合结果。
- [`product-firmware.yml`](../.github/workflows/product-firmware.yml) 在保守路由判断产品
  固件受影响时，为维护的产品源码分别构建两个芯片版本配置。
- [`arduino-policy.yml`](../.github/workflows/arduino-policy.yml) 检查当前零 sketch 清单和
  未来 `ChipVariant=prev3` 默认值，不声称执行过 Arduino 编译。
- [`repository-policy.yml`](../.github/workflows/repository-policy.yml) 执行确定性的配置、
  打包、路由和 Windows 烧录器契约测试。

这些工作流都会在每个 Pull Request 和推送到 `main` 时运行。路径过滤由仓库中受版本控制
的分类器完成，而不只依赖 GitHub 工作流触发器，因此仅文档改动也会产生稳定的必需检查。

## 改动路由

[`classify_changes.py`](../scripts/ci/classify_changes.py) 对 Pull Request 和 push 使用完整的
merge-base diff。遇到空范围、格式错误或不安全路径时会直接失败，不会静默退化为全量构建。

| 改动类别 | ESP-IDF 路由 |
| --- | --- |
| 根文档、docs、治理文件、原理图、图片或示例 Markdown | 不构建示例 |
| 单个直接示例内的源码或配置 | 只构建该示例 |
| 共享源码、根构建输入、工作流或 CI 辅助脚本 | 构建全部已发现示例 |
| 未知的非文档路径 | 构建全部示例并报告未知路径 |
| 内嵌 Brookesia `test_apps` | 不作为产品示例构建 |
| 已提交的固件/发布交付物 | 不做源码构建，标记交付评审 |

重命名会同时分类旧路径和新路径。旧写法 `example/ESP-IDF` 仅用于安全处理迁移和陈旧 diff；
规范工程根目录是 `examples/esp-idf`。

手动运行可传入 `all`、示例目录名（如 `04_wifistation`），或示例内部的仓库相对路径。
绝对路径、不存在路径和逃逸仓库的路径都会被拒绝。

分类器带有合成验收测试，覆盖仅文档、单工程、共享文件、工作流、固件、未知路径、重命名、
无效范围和手动选择器；这些测试会在生成构建矩阵前运行。

## 工程发现与构建矩阵

只有 `examples/esp-idf/` 的直接子目录且根目录含 `CMakeLists.txt` 时，才属于第一方工程。
组件内部的嵌套示例不会进入矩阵。本仓库对应单一产品，当前动态发现 12 个工程。

每个被选工程都以 `esp32p4` 为目标，并针对以下固定的 ESP-IDF 稳定标签编译：

- `v5.5.5`
- `v6.0.2`

标准示例矩阵为 12 个工程 × 2 个 ESP-IDF 版本 = 24 次构建，且全部使用 `rev1_3`
配置；不会为每个硅版本配置重复一套矩阵。当前 Arduino 清单为零，因此仓库不会声称或
运行 Arduino 构建。若以后加入 Arduino 表面，默认策略为 `ChipVariant=prev3`。

矩阵设置 `fail-fast: false`，最大并行数为 6。组件管理器下载与 ccache 会按运行器系统、
ESP-IDF 版本、目标、工程及依赖 manifest 哈希隔离。

## ESP32-P4 芯片版本配置

默认 ESP-IDF 配置为 `rev1_3`（v3 之前的芯片）：

- `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`
- `CONFIG_ESP32P4_REV_MIN_100=y`

`rev3_x` 配置用于 v3 或之后的芯片：

- `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n`
- `CONFIG_ESP32P4_REV_MIN_300=y`

不同配置使用独立的 sdkconfig 和构建目录，生成的二进制互不兼容，不能互相替用。维护的
产品源码为 [`12_esp32-p4-eye`](../examples/esp-idf/12_esp32-p4-eye/)，它在 ESP-IDF
v6.0.2 上产生独立的 `rev1_3` 与 `rev3_x` 产品任务和制品。这是产品专用兼容面，
不是将每个示例矩阵翻倍。

更新框架标签前，必须检查官方 ESP-IDF release 及旧版本到新版本之间的所有迁移指南。
当前矩阵的主要跨版本升级由官方
[ESP-IDF 5.5 到 6.0 迁移指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/migration-guides/release-6.x/6.0/index.html)
覆盖。

## 聚合结果

`ESP-IDF build matrix` 是稳定的最终门禁：

- 分类或路由测试失败时门禁失败；
- 没有选择工程时，只有构建任务确实为 skipped 才成功；
- 选择一个或多个工程时，只有生成的整个矩阵成功才成功。

分支保护应使用这个聚合作业。随着工程和框架版本变化，单个矩阵作业名称并不稳定。

## 构建制品

每个成功矩阵任务只打包该配置的 `flasher_args.json` 引用的文件；检出内容、制品名称和
manifest 都绑定 PR 分支的最终 HEAD，而不是 GitHub 的临时合并提交，并且都带有配置标识。
产品制品仅面向 ESP32-P4 主机：不包含 ESP32-C6 固件，禁止显式整片或区域擦除操作，并
拒绝越过 32 MiB 制品策略上限的烧录范围；这个安全上限不会改变产品参数表记录的 16 MB
外置 NOR Flash 容量。正常 `write_flash` 仍可能擦除它实际写入的扇区。打包器和烧录器
会分别校验每个 ESP 镜像头都是 ESP32-P4 芯片 ID 18，同时允许原始分区表、NVS 和数据项。
打包还会检查预期偏移、SHA-256 哈希和文件大小。制品包含：

- `manifest.json`：工程、目标、ESP-IDF 版本、提交、偏移、大小和 SHA-256；
- 可移植的 `flasher_args.json` 与 `flash_args`；
- 元数据实际引用的 bootloader、分区表、应用程序和其他二进制；
- `flash.sh` 与 `flash.bat` 辅助脚本。

制品保留 14 天。打包前会验证所有路径都位于所选配置的 build 目录内部。

生成的烧录器会在烧录前探测并再次探测 ESP32-P4 芯片版本：芯片版本低于 3 时仅接受
`rev1_3`，版本为 3 或更高时仅接受 `rev3_x`。芯片版本不能确定 PCB 版本或电气版本。

## Windows CI 固件测试流程

在与非 Draft Pull Request 最终 HEAD 完全一致的干净工作树根目录运行：

```text
Flash-CI-Firmware.cmd -SelfTest
Flash-CI-Firmware.cmd -ListOnly
Flash-CI-Firmware.cmd -Port COMx
```

交互式脚本需要 Git、已登录的 GitHub CLI，以及包含 `esptool` 的 Python 环境。它只接受
本地分支、开放且 Ready-for-review 的 PR、成功 Actions 与带配置标识制品的 SHA 全部一致
的结果。`-ListOnly` 显示完整的 26 项契约：24 份默认配置示例制品和两份维护产品配置。
实际运行时，v3 前芯片选择 24 份示例加 `rev1_3` 产品制品，共 25 项；v3 或之后的芯片
只选择一份 `rev3_x` 产品制品。

工具每次只下载并校验一份制品，重新探测芯片，并严格按 manifest 计划写入，然后停止。
它不会自动前进：只有完成当前固件所需的实机检查后，才能点击 **Mark PASS and flash
next**。进度同时绑定最终 SHA、制品构建 SHA、profile 和规范化 COM 端口；更换端口会重置
已确认进度。每次尝试、PASS 结果、下载包和日志路径保存在用户的本地应用数据目录中。
这些记录用于追踪人工顺序，但本身不等同于 HIL 证据。

## 不可混入源码 CI 的固件边界

[`firmware/ESP32-P4-WiFi6-LCD-3in5.bin`](../firmware/ESP32-P4-WiFi6-LCD-3in5.bin)
是独立交付的出厂镜像。源码 CI 不会构建、复制、封装或重新上传它。修改已交付的 `.bin`
或 `.zip` 时，分类器会要求明确的发布评审；维护者必须同时提供来源、版本、目标硬件、
刷写说明和硬件验证证据。

## 验证边界

Actions 通过只能证明所选源码工程能够使用记录的框架版本完成编译和打包，并非
硬件在环（HIL）证据。它不能证明 ESP32-C6 协处理器固件的运行时兼容性，也不能验证显示、
触摸、摄像头、音频、存储、USB、电源或无线功能；这些仍属于板级验收。本维护流程有意只把
提交后的 Actions 作为编译证据，不在本地执行 ESP-IDF 构建。
