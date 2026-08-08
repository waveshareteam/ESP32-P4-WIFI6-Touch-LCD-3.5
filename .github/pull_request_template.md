## Summary / 摘要

Describe the problem and the intended behavior.
请说明问题与预期行为。

## Scope / 范围

- Affected example or component paths / 受影响示例或组件路径：
- Board and hardware revision, if relevant / 相关开发板与硬件版本：
- Dependency or Kconfig changes / 依赖或 Kconfig 变化：

## Evidence / 证据

- [ ] `python3 scripts/check_readme.py`
- [ ] CI routing unit tests / CI 路由单元测试
- [ ] Final-sha Documentation workflow / 最终 SHA 文档工作流
- [ ] Final-sha `ESP-IDF build matrix` / 最终 SHA ESP-IDF 构建矩阵
- [ ] Hardware evidence attached when runtime behavior changed / 运行时行为变化时已附硬件证据

Actions proves compilation and packaging only. List the physical display, touch,
camera, audio, SD, USB, power, or ESP32-C6 hosted-Wi-Fi tests separately.
Actions 只证明编译和打包；请单独列出显示、触摸、摄像头、音频、SD、USB、电源或
ESP32-C6 hosted Wi-Fi 实机测试。

## Delivery boundary / 交付边界

- [ ] The checked-in factory firmware is unchanged / 仓库出厂固件未修改
- [ ] If delivery files changed, provenance, target, flash steps, hashes, and hardware validation are included / 若交付文件变化，已提供来源、目标、刷写步骤、哈希和硬件验证
