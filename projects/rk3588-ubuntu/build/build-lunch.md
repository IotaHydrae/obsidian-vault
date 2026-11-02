# 用 lunch 保存构建配置，再运行 build.sh

> `./build.sh lunch` 只选择并保存配置；随后不带参数运行 `./build.sh`，才执行已保存任务。
> 显式命令参数保留原有入口，供人工、agent 和自动化使用，不读取或修改菜单配置。

## TL;DR

- 普通 shell 编号菜单，无需 `whiptail`；`0` 或直接 Enter 接受默认项，各级输入 `q` 取消。
- 首次默认 `lubancat-5-v2`、`ubuntu-desktop`、完整镜像、eMMC；再次进入以上次保存值为默认。
- 配置位于工程根目录 `.build-config`，权限 `0600`，由 Git 忽略；清理 `build/` 不会删除它。
- `lunch --dry-run` 只预览，不保存、不构建；菜单本身不调用 sudo。

## 1. 选择、保存与执行

```sh
./build.sh lunch             # 选择后保存，不启动构建
./build.sh                   # 执行已保存的任务
./build.sh lunch --dry-run   # 预览选择对应的命令，不保存
```

菜单在普通 shell 中逐行输入，也支持标准输入；每个选项附简要说明。默认项显示为 `0（默认）`，其余选项按原顺序编号；
直接 Enter 等同于 `0`。重新进入菜单或返回修改时，当前选择继续作为默认项。
最终摘要的操作是 `0 保存`、`1 修改`、`2 查看命令`、`3 取消`；输入 `q` 也取消。
取消或预览保留已有配置及构建产物。`--help` 不要求交互终端。

无参数运行但尚未保存配置时，脚本提示先运行 `./build.sh lunch`，不猜测任务、不启动构建。
执行配置时按现有构建命令的要求使用 sudo；rootfs / 镜像仍在 RK3588 上原生构建，
内核 / U-Boot 仍在编译服务器构建，配置不会自动切换执行机器。

显式参数独立于保存配置，可继续直接用于 agent 与自动化：

```sh
sudo ./build.sh -b lubancat-5-v2
sudo ./build.sh -b lubancat-5-v2 -k
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=oem-host
```

## 2. 选择顺序

| 顺序 | 选项 | 行为 |
| --- | --- | --- |
| 板卡 | `lubancat-5-v2`、`lubancat-5io`、`rk3588-lsp-evb` | 从 `config/boards/*.conf` 读取列表；首次默认 5-v2 |
| 桌面 | `ubuntu-desktop`、`xfce4`、`kde` | 复用已有桌面逻辑；首次默认 ubuntu-desktop |
| 任务 | 完整镜像、从阶段继续、单独重做阶段、应用、内核、U-Boot | 阶段和应用在对应子菜单选择 |
| 启动介质 | eMMC、SD | 仅生成镜像的任务需要选择；首次默认 eMMC |

EVB 暂无实物，仅保留入口；其 DTS 仍有底板接线 TODO，硬件适配尚未完成，见
[EVB 接入与验证边界](derived-board-bringup.md)。

## 3. 配置文件与任务边界

根目录 `.build-config` 是纯 `key=value` 数据，字段为 `FORMAT=1`、`BOARD`、`DESKTOP`、
`TASK`、`STAGE`、`APP`、`MEDIUM`。读取时校验格式和选项，不使用 shell `source` 执行其内容。
它只保存当前工程的构建选择，不是阶段完成标记，也不代替 `build/` 中的缓存检查。

- **完整镜像**复用已有 deb，进入完整流水线。
- **从阶段继续 / 单阶段**映射到 `build-lsp-image.sh --from=<名称>` / `--stage=<名称>`；
  阶段含义及前置产物见[构建指南](build-guide.md#2-构建镜像)。
- **应用 / 内核 / U-Boot**复用 `build.sh` 的专用参数。ARM64 宿主未显式设置
  `CROSS_COMPILE` 时，为原生编译传空前缀。
- **eMMC**保留标准镜像；**SD**在镜像成功后调用 `tools/make-sd-image.sh`，显式指定本次板卡的
  源文件及 `-sd.img` 输出，覆盖同名旧 SD 产物；构建失败不进行转换。

SD 镜像使用 `boot-sd` / `writable-sd` 卷标，可与标准 eMMC 镜像共存，见
[介质标识冲突](../storage/multi-media-label-conflict.md)。配置执行只生成文件，不写物理设备；
NVMe 自动启动尚未接入，不列为可选介质。

切换桌面时，完整镜像或包含 `rootfs` 的任务在执行时检查现有桌面，必要时设置
`LSP_CLEAN_ROOTFS=Y`，从底包重建。仅更新上位机、安装板卡包或打包镜像无法切换桌面，
应先选择包含 rootfs 重建的任务；保存配置本身不清理 rootfs。

## 4. 验证与实现入口

```sh
python3 tests/build-lunch.py [--keep]
```

隔离测试通过 PTY 和管道标准输入驱动真实菜单，以构建记录器检查保存与执行分离、配置回读、显式参数隔离、
默认值与取消、预览、命令传递、失败传播和 SD 转换目标；不进行真实编译、安装或挂载。
具体测试边界见 tests/README.md（镜像源码：`tests/README.md`）。
入口为 `build.sh`、`scripts/build-lunch.sh` 与 `scripts/lib/build-config.sh`；阶段缓存见[构建内部机制](build-internals.md)。
