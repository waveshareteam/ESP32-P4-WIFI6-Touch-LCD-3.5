# 贡献指南

[English](CONTRIBUTING.md)

感谢改进 ESP32-P4-WIFI6-Touch-LCD-3.5 仓库。修改应保持产品针对性、可复现性，
并明确区分 CI 已证明的内容和实机硬件已证明的内容。

## 修改前

1. 检查当前分支、上游和工作区，保留与任务无关的用户修改。
2. 使用规范工程根目录 [`examples/esp-idf/`](examples/esp-idf/)。
3. 阅读 [CI 策略](docs/ci_ZH.md)；涉及组件时还需阅读[组件策略](docs/components_ZH.md)。
4. 同时依据产品原理图和相关 BSP 头文件核对硬件引脚，不要从相似型号开发板复制引脚表。

## 修改规则

- 第一方英文和简体中文文档必须同步更新。
- 除非仓库新增并维护 Arduino 工程，否则不要添加 Arduino 使用说明。
- 在 API、依赖、Kconfig、颜色顺序和硬件行为证明等价前，保留各示例内的 BSP 变体。
- 托管组件使用有边界的版本；精确版本或 IDF 条件版本应在 manifest 附近说明原因。
- 源码修改不得顺带编辑或重新生成仓库中的出厂镜像。固件交付必须有独立、经维护者确认
  的发布记录和硬件证据。
- 不要提交 `build/`、`managed_components/`、缓存、凭据、Wi-Fi 密码、私有本地路径、
  客户数据或包含秘密的串口日志。

## 静态检查

以下仓库检查不会编译固件，可在推送前执行：

```text
python3 scripts/check_readme.py
python3 -m unittest discover -s scripts/ci/tests -p "test_*.py" -v
python3 scripts/ci/classify_changes.py --working-tree
```

ESP-IDF v5.5.5 与 v6.0.2 的提交后 GitHub Actions 矩阵才是权威编译结果。如果 Pull
Request 没有标明目标板和已测试外设，请勿声称修改已经通过硬件验证。

## Pull Request

请保持单一主题，并提供：

- 问题与预期行为；
- 受影响示例路径和已知的硬件版本；
- 依赖或配置变化；
- 静态检查结果；
- 最终提交 SHA 对应的 Actions 结果链接；
- 显示、触摸、摄像头、音频、存储、USB、电源或 ESP32-C6 hosted Wi-Fi 修改的独立
  板级测试证据；
- 确认未修改出厂镜像；若固件交付本身就是任务范围，则提供完整发布来源。
