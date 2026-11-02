# VOP2 停用时设置 gamma 导致启动崩溃

> Rockchip 6.1 BSP 的 legacy gamma 回调必须在停用时仅缓存 LUT，不能访问 VOP 寄存器。
> 2026-10-14 在 LubanCat-5 V2 捕获到 Plymouth 设置 gamma 引发的异步 SError；复位后再次启动可恢复。
> 修复内核 `6.1.172-10` / `#10` 已生效：离线 gamma **20/20**、在线 gamma、按键开机与 DC 断电再上电样本均通过。
> 工程机旧内核 `#9` 的同一离线请求也触发 `0xbe000411`；自动重启后从 ramoops 取到完整崩溃栈。

## 故障与定位

开发板以 eMMC 启动，运行 `6.1.172 #9`。正常 `sudo poweroff` 后不切断供电，按电源键开机，
屏幕持续无信号、SSH 不通。串口抓到完整 DDR、SPL、BL31、U-Boot 启动，随后约 5.27 秒出现：

```text
CPU: 6 PID: 348 Comm: plymouthd
SError Interrupt ... code 0x00000000be000411
Kernel panic - not syncing: Asynchronous SError Interrupt
rockchip_drm_dbg_print
rockchip_drm_dbg
vop2_cfg_done
vop2_crtc_legacy_gamma_set
drm_mode_gamma_set_ioctl
```

这是整机内核崩溃，不能把该次无信号归为浏览器未启动。这次关机未走 `lsp-powerkey` 的两秒动作。
另一次没有串口记录的失败、以及工程机此前两次失败，不能仅凭相同屏幕现象认定为同一原因。

SError 是异步异常，报错 PC 不一定是发起总线访问的位置。这里紧邻日志调用的 `CFG_DONE` 写入
是具体候选，不能据 `rockchip_drm_dbg_print` 的名字把问题归为打印日志。

## 源码缺口与修复

依据 `kernel-6.1/drivers/gpu/drm/rockchip/rockchip_drm_vop2.c`：

- `vop2_disable()` 清除 `is_enabled`，释放 runtime PM 引用并关闭 VOP 时钟。
- `vop2_crtc_load_lut()` 在 `is_enabled=false` 时返回；旧 legacy gamma 回调随后仍无条件调用
  `vop2_cfg_done()`，并读取 standby。这两个访问都落在已经停用的硬件上。
- `drm_mode_gamma_set_ioctl()` 在持有 modeset 锁后调用驱动，允许停用 CRTC 的请求；
  modeset 锁不保证此前非阻塞 atomic commit 的尾阶段已经结束。

修复先等待目标 CRTC 的既有提交完成，再获取 enable/disable 共用的 `vop2_lock`。
LUT 始终缓存并设置 `gamma_lut_active`；只有共享 VOP、目标 CRTC 和 VP 均启用且 LUT 映射存在时
才执行硬件访问。下次 `vop2_crtc_atomic_enable()` 使用现有恢复路径加载缓存。
在线 gamma 的帧同步保持原有行为；等待提交必须在获取 VOP 锁之前，避免阻塞需要该锁的提交尾阶段。

启动时序影响 gamma 请求是否落在 VOP 停用的窗口里，所以正常启动可能偶发失败。
根本缺口是驱动未保护停用状态下的硬件访问：先关闭 VOP、再提交一次 gamma，无需两个请求同时执行
就能确定性触发。该对照验证相同漏洞，不等于重放了此前每一次失败启动的全部时序。

当前 Plymouth `24.004.60-1ubuntu7.2` 的 `src/plugins/renderers/drm/plugin.c` 中，
`ply_renderer_head_set_scan_out_buffer()` 先设置 gamma，再调用 `drmModeSetCrtc()`。
gamma 对同一 head 仅提交一次，未检查返回值便释放数据，不会因普通 ioctl 错误重试；
panic 更无法返回用户态。修复后的停用请求缓存成功并返回 0，下次 enable 回放，恢复不依赖 Plymouth 重试。

## 验证结果与边界

实测（2026-10-14，LubanCat-5 V2，eMMC 启动）：

- 旧内核 `#9`：关闭全部 CRTC 后确认 VOP 为 `suspended`，第一次 `SETGAMMA` 即触发
  同一 `0xbe000411` SError。该次异常位于 standby 的 `readl`，证明旧回调在停用状态仍访问 MMIO；
  panic 后自动重启恢复。
- 工程机 `#9`：CRTC 75、gamma_size 1024，关闭后两次确认 VOP 为 `suspended`，
  首次 `SETGAMMA` 即触发同一 `0xbe000411`。ramoops 的 console 记录保留完整栈，
  路径为 `vop2_crtc_legacy_gamma_set → vop2_cfg_done → rockchip_drm_dbg`，与开发板启动故障一致。
  临时设置 panic 10 秒自动重启；重启后 kiosk 为 `active`，已撤回临时 sysctl 文件并恢复原值 0。
- 修复内核 `6.1.172-10` 已安装并重启，`uname -v` 为 `#10`。真实离线 gamma **20/20 通过**，
  每次 GETGAMMA 均与提交值一致，且每次 ioctl 前后 VOP 均为 `suspended`。
  恢复图形栈后 kiosk 为 `active`，VOP `runtime_status` 为 `active`。
- 同一内核的在线回归：保持 DRM master，停用全部 CRTC 后提交 reverse LUT，重新启用
  CRTC 75（3840×2160）并确认 VOP 为 `active`，在线提交 identity LUT、GETGAMMA 一致；
  再停用、确认 `suspended` 并恢复原 gamma，退出后 kiosk 恢复。此项核对 ioctl 与软件缓存，
  不验证硬件 LUT 内容或显示颜色。
- 正常 `sudo poweroff` 后保持供电、HDMI 连接，轻按 Power 键启动一次通过；串口未捕获
  SError/panic，运行 `#10`、kiosk 为 `active`、上位机 HTTP 200，现场确认屏幕正常。
- 正常关机后拔掉 DC 12V，约 5 秒后重新插入，自动开机一次通过；同样运行 `#10`、
  kiosk 为 `active`、上位机 HTTP 200，串口和内核 journal 未捕获 SError/panic，现场确认屏幕正常。
  这是该 V2 板插入 DC 后自动开机的路径，未按 Power 键。
- 工程机刷入新镜像后从 eMMC 启动，实时核对为 `6.1.172 #10`，image / headers 包均为
  `6.1.172-10`；HDMI 已连接，kiosk、上位机服务与 socket 均为 `active`，HTTP 200。
  本次内核 journal 未检出 SError/panic；未在工程机重跑停用 gamma 注入。

`tests/vop2-legacy-gamma.py` 默认只读列出 CRTC。显式 `--test-off` 会关闭全部 CRTC，
要求绑定 VOP2 的 `runtime_status=suspended`，再提交不同 gamma 并核对读回的缓存。
必须提前暂停 HDMI 调度、停止 Cage/GDM、退出 Plymouth；结束后由调用方恢复图形栈。
旧内核可能立即 panic，运行前保留串口抓取和可用的恢复方式。

```sh
sudo python3 tests/vop2-legacy-gamma.py --test-off --iterations=20 --keep
```

该测试证明停用期间 ioctl 接受、缓存正确且 VOP 保持 suspended，不证明屏幕上的颜色或硬件 LUT 回放。
实际硬件 LUT 内容与启动期 Plymouth gamma 调用未直接追踪。
多次成功启动只是对应样本，不能作为所有电源路径或长期稳定性的保证。
