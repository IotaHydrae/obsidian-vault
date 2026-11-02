# BL32（OP-TEE）：怎么开、怎么验、什么时候才值得开

> 状态（2026-09-18）：**默认不开**；开关与真机验证都已完成。
> 本文是"要开的时候照着做"的操作手册 —— 构建怎么开、产物怎么验、上板怎么试（不碰 eMMC）、怎么回滚。
> 背景原理（BL31/BL32 在启动链里各干什么、跑在哪个 EL）见 [rk3588-boot-flow.md](rk3588-boot-flow.md) §3.7；
> 上游同步与"为什么默认关"的评估见 [../build/upstream-sync.md](../build/upstream-sync.md) §2.1。

---

## TL;DR

- 状态（2026-09-18）：**默认不开**（BL31-only）；开关与真机验证都已完成 ⇒ 要开时照本文做。
- 先判断"该不该开"（§2）：需要 OP-TEE 那条链（安全世界服务 / TA）才值得，**我们目前不需要**。
- 开之前**内核与设备树不需要改**（已就绪，§3）；怎么开见 §4。
- 上板验证走 **SD 卡法（不碰 eMMC）**，见 §5；回滚见 §6。新增的那行是 `Checking optee … + OK`。
- ⚠️ 目前能测到哪一层、真要"用起来"还差什么，见 §7；坑见 §8。

## 0. 结论先看

| 问题 | 答案 |
| --- | --- |
| 现在开吗？ | **不开**（默认 BL31-only） |
| 想开怎么做？ | `sudo UBOOT_WITH_BL32=1 ./build.sh -b <board> -u` —— 一条命令 |
| 要改内核/设备树吗？ | **不用**（`CONFIG_TEE=y`/`CONFIG_OPTEE=y` 都有；DT `/firmware/optee` 已启用） |
| 开了有什么变化？ | FIT 多一个 `(optee)` 节点；启动后 `/dev/tee0` 出现；**`MemTotal` 少 16 MB** |
| 怎么确认开了？ | 见 §4.2（构建侧）与 §5.4（运行时），两边都有硬判据 |
| 开了能测到哪一层？ | 版本/共享内存/建关会话/**加载厂商 TA** 都通（§7 表），一条命令复测：`tests/tee-probe.sh` |
| 要在产品里真的"用"起来还差什么？ | ①装 `tee-supplicant`（**不装就会不可中断卡死**）②拿到 TA 的命令号（厂商文档没公开，我们只建会话、不 invoke）③自研 TA 要用厂商密钥签 |
| 默认产物受影响吗？ | **不受影响**（实测：默认仍是 3 个 atf 节点、无 optee，与以前逐项一致） |

---

## 1. BL32 是什么、卡在启动链的哪一步

```text
BootROM → ② DDR 固件 → ③ U-Boot SPL(=BL2) → ④ BL31(EL3, 常驻)
                                                 ├─(可选) ⑤ BL32/OP-TEE(S-EL1, 常驻)
                                                 └→ ⑥ U-Boot proper(=BL33, EL2) → ⑦ 内核(EL1)
运行时：内核/用户态 --smc--> ④(PSCI/SIP)；内核 optee 驱动 --smc--> ⑤(有才在)
```

- **BL32 = OP-TEE OS**，是**安全世界（S-EL1）里的一个独立小操作系统**，给正常世界提供 TEE 服务：
  安全存储（RPMB）、密钥与加解密、DRM（Widevine L1 之类）、secure video path、厂商 TA。
- **它不是启动流水线上的"下一站"**：BL31 一直在 EL3，只是**额外**把 BL32 初始化到 S-EL1，
  然后自己继续退出到正常世界。④⑤ 是**并行的常驻服务**，靠 SMC 按需调用。
- **它在 FIT 里长这样**：`atf-1/atf-2/atf-3` 是 **BL31 那一个 ELF 的三个 `PT_LOAD` 段**（不是三个固件），
  `optee` 是**第 4 个独立节点**（`type = "firmware"`、`os = "op-tee"`），
  并会被写进 `configurations` 的 `firmware = "optee"` / `loadables` 里 ⇒ BL31 看到它才会去初始化。
- 内核侧通道：DT `/firmware/optee { compatible = "linaro,optee-tz"; method = "smc"; }`
  → 内核 `optee` 驱动 → SMC → BL31 的 `opteed` → BL32。

---

## 2. 先判断"该不该开"（我们目前是"不该"）

| 潜在消费者 | 现状 | 要 BL32？ |
| --- | --- | --- |
| 镜像用户态 | 仓库/镜像里**没有任何** `optee-client`/`tee-supplicant`/`libteec` 引用 | ❌ 装了也没人调 |
| 内核 `CONFIG_OPTEE=y` | 驱动在，无 BL32 时 probe 失败（`api uid mismatch → -22`，无害） | ❌ |
| 内核 `rockchip-secure-otp` | 内核里**唯一真走 OP-TEE** 的驱动；但 **RK3588 的 DT 里没有 `secure-otp` 节点** ⇒ 永不 probe | ❌ |
| `CONFIG_TRUSTED_KEYS=m` / `optee-rng` | 没有"无 TPM 密封密钥"需求；SoC 自带 TRNG | ❌ |
| HDCP2.2（`CONFIG_ROCKCHIP_DW_HDCP2=y`，HDMI RX） | 驱动里没有任何 tee/optee/secure 引用 | ❌ |
| Secure Boot | 走 OTP eFuse + FIT 签名，**不需要 OP-TEE**；反过来上了它还得多签一个镜像 | ❌ |
| Widevine / 受保护播放 | Chromium L3 是软件实现；我们也不播受保护内容 | ❌ |

**代价**：`MemTotal` **−16,352 kB（≈16 MB）**（实测）、多一个要跟版本的二进制、启动链多一环、
安全世界 TCB 变大；将来开 secure boot 还要给 BL32 签名；真要**用**还得装 `tee-supplicant` + 准备 TA。

**命中任一条就该开**：① 受保护内容通路（Widevine L1 / HDCP / secure video path）；
② 用 TEE 存密钥或做 attestation（RPMB、`trusted.keystore`）；③ 厂商必需功能/SDK 要求；
④ 要在 Linux 里读 secure OTP（还要先给 RK3588 的 DT 补 `secure-otp` 节点）。

---

## 3. 开之前：内核与设备树**不需要改**（已就绪）

```console
$ grep -n 'CONFIG_TEE=y\|CONFIG_OPTEE=y' kernel-6.1/arch/arm64/configs/rockchip_linux_defconfig
2353:CONFIG_TEE=y
2354:CONFIG_OPTEE=y
$ sed -n '55,61p' kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-linux.dtsi
	firmware {
		optee: optee {
			compatible = "linaro,optee-tz";
			method = "smc";
			//status = "disabled";      ← 已注释掉 = 节点是启用的
		};
	};
```

⇒ 唯一要动的是 **bootloader 那一层**（把 BL32 打进 FIT）。

---

## 4.~6. 怎么开、怎么上板验证、怎么回滚 → 见 [bl32-optee-enable-and-verify.md](bl32-optee-enable-and-verify.md)

一条命令开启与产物验证、开关的 4 处实现、备选做法、SD 卡上板验证法与回滚**已搬成单独一篇** →
**[bl32-optee-enable-and-verify.md](bl32-optee-enable-and-verify.md)**。


## 7. 现在能测到哪一层 / 真要"用起来"还差什么

**已经实测通过的（2026-09-18，`lubancat-5-v2`）**：

| 层 | 测什么 | 判据/证据 |
| --- | --- | --- |
| FIT 打包 + SPL 校验 | `optee` 节点有没有真的进 FIT 并被逐个校验 | 串口 `## Checking optee 0x08400000 ... sha256(76eeb0b48d…) + OK` |
| BL31 → BL32 初始化 | 安全世界起来了 | `INFO: BL31: Initializing BL32` + OP-TEE banner（版本/内存布局）+ 不再有 `No OPTEE` 报错 |
| 内核 ↔ OP-TEE | SMC 握手与驱动初始化 | `optee: revision 3.13 (1e844e4c)`、`dynamic shared memory is enabled`、`initialized driver` |
| 设备节点 | TEE 字符设备 | `/dev/tee0`、`/dev/teepriv0` 出现 |
| 用户态 ABI | `TEE_IOC_VERSION` | `impl_id=1 (OPTEE)`、`gen_caps=0xd`（GP/REG_MEM/MEMREF_NULL） |
| 用户态 → 安全世界建会话 | `TEE_IOC_OPEN_SESSION` | `system PTA` 返回 `TEE_ERROR_ACCESS_DENIED`、伪 UUID 返回 `TEE_ERROR_ITEM_NOT_FOUND`（都是 TEE 层返回码） |
| 内存代价 | 安全世界预留 | OP-TEE 自报 `TEEOS 2M + TA 12M + SHM 2M`；`MemTotal` −16,352 kB |
| 与 BL31 共存 | CPU 热插拔（PSCI） | `cpu7` offline→`0-6`、online→`0-7`（8 核），`optee` 日志无新增异常 |
| 系统无回退 | Type-C/USB 挂起/温度/启动耗时 | 全部与不带 BL32 时一致 |
| 共享内存 | `TEE_IOC_SHM_ALLOC` + `mmap` + 写读回 | `rc=0 fd=4 size=4096 id=1 mmap=ok`（⚠️ ioctl 的**返回值就是新 fd**，要在它上面 mmap，见 §8） |
| **动态 TA 加载** | TA 从文件系统加载 → 验签 → 进 TA RAM → 建会话 | 把上游 SDK 的厂商 TA `1db57234-…e2a5.ta`（80,196 B，sha256 `42551e51…`）放进 `/lib/optee_armtz/`，`TEE_IOC_OPEN_SESSION` → `TEE_SUCCESS`、`CLOSE rc=0` |
| **invoke TA 命令**（唯一有出处的命令号） | Linux → SMC → BL31 → OP-TEE → TA 真的执行 | cmd 102：`INVOKE rktest cmd=102 rc=0 ret=0x00000000 TEE_SUCCESS origin=4 a=67 b=67 value=pass buffer=pass` |
| 反复建/关会话 | 有没有泄漏/退化 | 临时 `tee-supplicant` 下 **50 次开+关会话全成功，平均 6.3 ms/次** |
| 一条命令复测 | 回归脚本 | **`tests/tee-probe.sh`**：板上 **通过 16 / 失败 0 / 警告 0 / 跳过 0**（`--uuid=1db57234… --ta-test --soak=50`），退出码 0 |

**那个 cmd 102 是哪来的、为什么敢调**：厂商自己的 U-Boot 源码里就写着 ——
`OpteeClientInterface.c` 里的 `trusty_oem_user_ta_transfer()`：
⚠️ 这是**厂商 SDK 里那份**（约 1937 行）；⛔ 我们仓库 `u-boot/lib/optee_clientApi/OpteeClientInterface.c` 只有 1400 行、
**没有**这个函数 —— 别把它当仓库内可检索的文件。函数里是：
`TEEC_InvokeCommand(&TeecSession, 102, …)`，参数是
`param0 VALUE_INOUT (a=66)`、`param1 MEMREF_INPUT "Transfer data test."`、
`param2 MEMREF_OUTPUT` 同长度缓冲；厂商源码 1931/1937 行的判据是「出来 `a==66+1 && b==a`」与
「输出 buffer 与输入逐字节相同」。我们实测两项都 pass ⇒ **TA 真的被调用并执行了**，
而且这条命令只做寄存器和内存里的加法与拷贝（不碰 OTP/RPMB/存储）。
⛔ **这是唯一敢用的命令号**：其它 TA（`rk_os_service.ta` 有 `rk_write_oem_otp`，
`librk_tee_service` 有 `rk_otp_write`）**一个命令号都没有公开文档，不许猜、不许试**。

**还需要补东西才能测的**：

1. **自签 TA**：厂商**自己那套密钥签的 TA 已经能加载了**（上表最后三行，用的就是上游 SDK 里那份
   user TA），说明 BL32 内建的公钥与厂商 TA 是配套的。⚠️ 但**我们自己用另一对密钥签的 TA 会被拒**
   （`TEE_ERROR_SECURITY`）—— 想让自研 TA 上线，要么用厂商的私钥（要向野火/瑞芯微要），
   要么把 BL32 换成用自研密钥/开发密钥重编的（要自建 OP-TEE，属于另一件事）。
   ⚠️ **`tee-supplicant` 是必需品，不是可选项**：没有它时任何 `TEE_IOC_OPEN_SESSION` 都会让调用进程
   **不可中断阻塞**（D 状态，见 §5.4 第 2 条）。**所以将来真要把 BL32 用到产品里，镜像必须同时装 supplicant。**
   - 上游那份 user TA 的来源：`u-boot/lib/optee_clientApi/userta/`（在**上游 clone** `~/example-project/ubuntu-ebf-rk3588`，
     **不在本仓库**；⚠️ 文档里早先写的 `/home/board/example-project/…` 是当年容器里的路径，**已不存在**）；
     同目录还有一份 178,000 B 的同 UUID TA（另一套构建，未测）。
     TA 文件名的 UUID **带不带连字符两种都能被加载器认**，但**同一 UUID 不要放两份**（会撞）。
   - ⛔ **只建/关会话，不要 invoke**：厂商 TA 里混着写命令，详见本页开头与 §9。
2. **厂商内建 TA**：这颗 BL32 里能看到 `rk_crypto_service.ta`、`rk_os_service.ta`、`stats.ta`
   （以及 `rk_get_trng`、`rk_set_register`、`stats.ta get_alloc_stats` 等命令串），
   还有 HDCP key 相关（`rk_hdcpkey_init`、`storage_oem_hdcp_key_is_written`）。
   它们看起来是**编进 OP-TEE 的 early TA**（不需要 supplicant 也能开会话），要跑起来需要拿到它们的
   **UUID 与命令号**（二进制里的 UUID 是 16 字节裸数据，没跟名字放一起；驱动只把 DT 里登记的 TA 挂到
   `/sys/bus/tee/devices/`，而我们板子上那里是空的）。
   ⚠️ 顺带注意：`rk_os_service.ta` 里有 **`rk_write_oem_otp`** 这类命令 ⇒ 它也是"能写 OTP"的一条潜在路径
   （**只有被显式调用才会写**，见 §9）。
3. **RPMB / 安全存储**：需要 TA + `CFG_RPMB_FS`；RPMB 首次使用会在 **eMMC 的 RPMB 分区**写密钥
   （不是 OTP，但**会改板子**）⇒ 测之前先跟用户确认。
4. **受保护播放（Widevine L1/HDCP）**：需要厂商 TA + 用户态 + 内容侧配合，现在不具备。

**真要"用起来"还差什么**：

1. **用户态**：装 `tee-supplicant`（noble 仓库里有，`apt-cache policy tee-supplicant` = **4.1.0-1.1**）
   + 用户程序用的 `libteec`；以及要用的 **TA**（可信应用）。
   ⚠️ 注意**版本搭配**：BL32 里的 OP-TEE **OS 是 3.13**（内核日志里 `optee: revision 3.13`），
   而 noble 的客户端是 4.1.0 —— 基础 TA 加载/调用通常没问题，但要用新特性前先实测。
2. **权限**：`/dev/tee*` 是 `crw------- root:root`，且系统里**没有 `tee` 组**；
   非 root 程序要用得自己定 udev 规则/加组（我们没有默认加）。
3. **DT**：`/firmware/optee` 已经就绪，不用改。若要在 Linux 里读 **secure OTP**，
   还得给 RK3588 的 DT 补 `secure-otp` 节点（`drivers/nvmem/rockchip-secure-otp.c` 经 OP-TEE 访问，
   见 [otp-secure-boot.md](otp-secure-boot.md) §1）。
4. **镜像构建**：目前 `BASE_PACKAGES` 里没有 tee 相关包 —— 真要上就把需要的加进去
   （加包属于构建改动，按 AGENTS §2 的规矩先问）。

---

## 8. 坑（都踩过或有实测依据）

1. ⛔ **"给了 `TEE=` 才打进 FIT"这条判断，上游的位置在 arm64 上是坏的**：
   上游把早退写在 `if [ "${ARCH}" == "arm" ]` **里面**，而 arm64 板子（`CONFIG_ARM64=y` ⇒ `ARCH="arm64"`）
   **进不去那个分支** ⇒ 不给 `TEE` 时会生成 `/incbin/("tee.bin")`，然后
   `mkimage: FATAL ERROR: Couldn't open "tee.bin": No such file or directory` ⇒ **整个 u-boot 构建失败**。
   上游靠"`rules` 无条件传 `TEE=`"掩盖了这个 bug。**我们已经把判断挪出 ARCH 分支**；
   ⚠️ **将来整棵升级 u-boot 时，只要我们还保持"默认关 BL32"，这条修必须重新打**
   （记录在 [../build/upstream-sync.md](../build/upstream-sync.md) §4 的"必须重打的本地改动"里）。
2. ⛔ **`_bl32` 缺了会静默不生效**：`TEE=` 会展开成空 ⇒ `gen_bl32_node` 直接 return ⇒ FIT 里没有 optee
   而构建照常成功。所以 `scripts/build-u-boot.sh` 里加了"开了开关必须有 `(optee)` 节点"的校验；
   自己手动 `dpkg-buildpackage` 时务必按 §4.2 验一遍。
3. ⚠️ **TEE 地址是算出来的**：`TEE_LOAD_ADDR = CONFIG_SYS_SDRAM_BASE + 0x08400000`。
   RK3588 的 `CONFIG_SYS_SDRAM_BASE = 0` ⇒ `0x08400000`。换 SoC/换内存基址时要重新核。
4. ⚠️ **内存代价是实打实的**：实测 `MemTotal` −16,352 kB。别在内存紧张的方案里随手开。
5. ⚠️ **别把 `atf-*` 当成 BL32**：`atf-1/2/3` 永远是 BL31 的三段；BL32 是 `(optee)` 那个节点。
6. ⚠️ **判新旧别看 SPL 横幅的日期**（增量编译不重建 `spl/u-boot-spl.bin`），
   看 FIT 的 `fwver:`、节点列表与各节点 sha256。
7. ⛔ **工程机的 eMMC 不要动**（AGENTS §2.13）；上板验证一律走 §5 的 SD 卡法或先问用户。
8. ⚠️ 上了 BL32 之后，**将来若开 Secure Boot，BL32 也要一起签名**（多一个签名对象/一步）。
9. ⛔ **`TEE_IOC_SHM_ALLOC` 的返回值就是共享内存的新 fd**（内核 `tee_shm_get_fd()` →
   `anon_inode_getfd("tee_shm", …)`），**不是** id/偏移：要在**那个 fd** 上 `mmap(…, offset=0)`。
   拿 `/dev/tee0` 的 fd + `shm.id` 当 offset 会 `mmap` 失败 —— 我们第一版探针就是这么错的
   （输出 `SHM_ALLOC rc=0 size=4096 mmap=fail`，还顺带漏掉一个 fd）。
   判"真能用"不能只看 `mmap` 返回，要**写一个字节再读回**。
10. ⚠️ **`set -o pipefail` 下 `dmesg | grep -q …` 会假阴性**：`grep -q` 命中即退出 ⇒ `dmesg` 吃
    SIGPIPE 退出 141 ⇒ 整条管道被判失败（明明命中了却走 else 分支，我们就被坑了一次）。
    正确做法：`DMESG="$(dmesg 2>/dev/null || true)"` 取一次，再用 `case "$DMESG" in *…*)`/`printf | grep`。
11. ⛔ **invoke 的 memref 参数不能传用户指针**：内核 `params_from_user()` 只认
    `param.c` = `TEE_IOC_SHM_ALLOC` 拿到的 **shm id**（`tee_shm_get_from_id(ctx, ip.c)`，
    `kernel-6.1/drivers/tee/tee_core.c:393`），`param.a` 是**该 shm 内的偏移**。
    传裸指针 + `c=0` 会直接 `-EINVAL`（实测踩到：`INVOKE rktest cmd=102 rc=-1 errno=22`）。
    即 **TEEC_MEMREF_TEMP_\* 是 libteec 在用户态用 SHM_ALLOC 模拟出来的**，内核这层不认裸指针。
    另外 `TEE_IOC_INVOKE` 的 `buf_len` 同样要求**严格等于**
    `sizeof(struct tee_ioctl_invoke_arg) + num_params*sizeof(struct tee_ioctl_param)`。
12. ⚠️ **TA 文件名的 UUID 带不带连字符都能被加载器认**（`<uuid>.ta` 两种写法都扫），
    但**同一 UUID 不要放两份**（会撞）。

---

## 9. 附：开 BL32 **会不会烧 OTP（eFuse）**？—— 不会

（OTP 一次写不可逆，所以这条单独说清楚。字段地图与"只读"铁律见 [otp-secure-boot.md](otp-secure-boot.md)。）

**这次改动实际写到的东西**：SD 卡的裸 loader 区（`seek=64` / `seek=16384` 两处）、板子 `/tmp` 里的一个文件、
仓库里的文件。**没有任何 eFuse 访问**；当时 eMMC 一个字节没动（其 loader 那时仍是 v1.47；
2026-09-18 之后 eMMC 的 `u-boot.itb` 已被换成 v1.53 BL31-only，见 [bl31-runtime-and-upgrade.md](bl31-runtime-and-upgrade.md) §6.3）。

系统里**能烧 OTP 的就 4 条路径**，逐条看在我们这里是关的：

| # | 路径 | 代码位置 | 触发条件 | 我们这里 |
| --- | --- | --- | --- | --- |
| 1 | 烧 **Secure Boot 公钥哈希** + 使能位（`0x9c0` / `0x20`） | U-Boot `lib/rsa/rsa-verify.c:594 rsa_burn_key_hash()`，调用点 `common/image-sig.c:442` | 编译守卫 `CONFIG_SPL_BUILD && CONFIG_SPL_FIT_HW_CRYPTO && CONFIG_SPL_ROCKCHIP_SECURE_OTP`，**且**必须走到"**验证签名 FIT 成功**"那一步 | ⛔ **进不去**：`CONFIG_FIT_SIGNATURE` / `CONFIG_SPL_FIT_SIGNATURE` **未设置**（实测 `.config`），我们的启动是 `boot.scr` + `booti` 裸内核、FIT 里也没有 `signature` 节点（启动日志 `Verified-boot: 0`） |
| 2 | 写 **OTP 回滚计数器** | U-Boot `arch/arm/mach-rockchip/spl.c:458 fit_write_otp_rollback_index()`，调用点 `spl_board_prepare_for_jump()` | `#ifdef CONFIG_SPL_FIT_ROLLBACK_PROTECT` | ⛔ **代码都没编进去**：实测 `.config` 里 `CONFIG_SPL_FIT_ROLLBACK_PROTECT` **未设置**；而且我们的 FIT 没有 rollback index |
| 3 | OP-TEE 里的 OTP 服务（二进制里有 `core/arch/arm/plat-rockchip/{common/otp_support.c,rk3588/rk_otp.c}`、PTA `core/pta/rockchip/uboot_storedata_otp.c`、以及 `Disable oem otp write from %d to %d …` 这类串） | BL32 内部 | **被显式调用才执行**（正常世界经 SMC/PTA 请求） | ⛔ **没人调**：U-Boot 里 grep 不到任何 PTA/`storedata` 调用；内核与用户态也没有 TA 去调（镜像里连 `tee-supplicant` 都没有）。⇒ 能力常驻但不执行 |
| 4 | 内核侧直接写 | `drivers/nvmem/rockchip-otp.c` / `rockchip-secure-otp.c` | nvmem 的 write 回调 | ⛔ NS OTP 驱动**只读**（`read_only = true`、`rk3588_data` 里没有 `.reg_write`）；**secure-otp 驱动在 RK3588 上连 DT 节点都没有** ⇒ Linux 够不到 |
| 5 | 厂商内建 TA 里的 **`rk_write_oem_otp`**（在 `rk_os_service.ta` 里；同一颗 BL32 还有 `rk_hdcpkey_*`、`storage_oem_hdcp_key_is_written` 等） | BL32 内部的 TA | **必须被显式调用**（正常世界开该 TA 的会话并 invoke，还要过签名/权限） | ⛔ **没人调**：镜像里没有 supplicant、也没有 TA 调用方；写的是 **OEM OTP**（与 Secure Boot 的 `0x20`/`0x9c0` 不是同一块）。我们**只建会话、绝不 invoke** —— 厂商 TA 的命令号没有公开文档，猜错就有误写风险（§7 已测到 `librk_tee_service` 的 UUID `4367fd45…` 与 user TA 的 UUID，但**命令号一个都不知道**） |

**实测旁证**（开发板，只读）：

```console
$ head -c 32 /sys/bus/nvmem/devices/rockchip-otp0/nvmem | od -An -tx1 -v
 52 4b 35 88 12 fe 21 4e 59 38 34 50 00 00 00 00
 00 00 00 00 05 06 08 08 08 09 21 0d 08 00 00 00
```

与 [otp-secure-boot.md](otp-secure-boot.md) §3 里 **2026-09-16 记录的那份逐字节一致** ⇒ 整个 BL31/BL32
升级与上板测试过程中，NS OTP 没有任何变化（文件 mtime 变了只是本次启动重新创建了 sysfs 节点）。

**真正会烧 OTP 的是"显式的产线动作"**：手工执行 `misc_otp_write`（U-Boot 命令）、走签名 provisioning 流程
（`rsa_burn_key_hash()` 会先回读 `0x9c0`，非零就报 `RSA: The secure region has been written.` 拒绝重写）、
或用 RKDevTool 之类的工具操作 eFuse。这些我们都没有、也不该做（AGENTS「OTP 只读」）。

> 想"眼见为实"也可以：现在 BL32 在跑，理论上可以让内核**经 OP-TEE** 读 secure OTP
> （给 RK3588 的 DT 补 `secure-otp` 节点即可，读是安全的）——但那要改 DT + 重启，属可选项，不是必需。

## 10. 相关文档

- [bl31-mechanism.md](bl31-mechanism.md) —— **BL31（EL3）机制详解**：PSCI/SIP 接口全表、opteed 通道、复位/关机、内存地图
- [rk3588-boot-flow.md](rk3588-boot-flow.md) §3.7 —— 全链路总表：谁跑在哪个 EL、是否常驻、BL31/BL32 各干什么
- [../build/upstream-sync.md](../build/upstream-sync.md) §2.1 —— 为什么默认关（消费者排查表）、开关实现、双向实测、踩坑
- [../build/upstream-sync.md](../build/upstream-sync.md) §6.1/§6.2 —— 上板测试清单与"SD 卡带新 loader + 系统走 eMMC"的完整实测记录
- [otp-secure-boot.md](otp-secure-boot.md) —— OTP/eFuse、Secure Boot 与（安全 OTP 需要 OP-TEE 的）关系
- ../../tests/tee-probe.sh（镜像源码：`tests/tee-probe.sh`）（+ `tee-probe.c`）—— §7 那张表的**复现脚本**：
  一条命令把版本/共享内存/会话/TA 加载全测一遍，并对"没有 supplicant"做安全跳过（见 ../../tests/README.md（镜像源码：`tests/README.md`））
- ../../packages/u-boot-lubancat-rk3588/debian/rules（镜像源码：`packages/u-boot-lubancat-rk3588/debian/rules`） —— 开关本体（`UBOOT_WITH_BL32`）
