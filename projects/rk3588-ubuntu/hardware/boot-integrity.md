# 启动校验与防篡改：改了一块会不会被发现

> **能发现"被改"的只有 FIT 内部的 sha256（`u-boot.itb` 各节点）** —— 而且它是**完整性校验**，
> 不是**真实性校验**（没有签名，改完重算哈希一样能过）。想真防篡改要一整条链（见下）。

## TL;DR

- `u-boot.itb` 是 **external data** 布局的 FIT：头部 FDT + 各子镜像 payload（512 字节对齐），
  每个节点带 sha256 ⇒ SPL 加载时会核对（`boot_fit` 侧）。
- ⛔ **这是检测"意外损坏"，不是"防篡改"**：没有签名、没有公钥，谁能改文件谁就能改哈希。
- 想真防篡改要**整条链**（BootROM 信任根 → 签名过的 loader → 验签的 FIT → dm-verity 的 rootfs），
  只补中间一层没有意义 —— 见下文那节。
- 我们**没有**启用安全启动/OP-TEE 的这条链（BL31-only，`git` 里的 OTP 也是只读不写，见
  [otp-secure-boot.md](otp-secure-boot.md)）。
- 下文附录是这次分析引用的关键代码（原样摘录）。

## 启动时的校验过程 —— 能发现"被改"吗？

**机制（FIT 哈希，SPL 阶段）**。`u-boot.itb` 是 **external data** 布局的 FIT：FDT 头在文件最前，
各子镜像的 payload 依次排在 `FIT_ALIGN(fdt_totalsize)`（`IMAGE_ALIGN_SIZE=512`，即 512 字节对齐；
本文件实测基址 `0x800`）之后，节点里只留 `data-offset`/`data-size`。每个子镜像节点下都有一个
`hash { algo = "sha256"; value = <32 字节>; }`。

SPL 按 `data-offset` **直接从存储介质读** payload（`common/spl/spl_fit.c:210`），**现场重算 sha256**
再和 FIT 里记的 `value` 比（`fit_image_check_hash()`，`common/image-fit.c:1299`）：

- 日志里 `sha256(6a4a192c10...)` 里的 10 个 hex 是 **SPL 现场算出来的哈希的前 5 字节**
  （`common/image-fit.c:1355-1360`，只在 `CONFIG_SPL_BUILD` 下打印），`+ OK` = 与 FIT 里记的值一致；
  不一致会打 `Bad hash: <完整计算值>` 然后停下来；
- `Trying fit image at 0x4000 sector` = 在 **8 MiB（扇区 0x4000）** 处找到 FIT 头（对应
  [rk3588-boot-flow.md](rk3588-boot-flow.md) §2.1 的存储布局）。

**它能证明什么、不能证明什么**（关键）：

| 能 | 不能 |
| --- | --- |
| 板上这一份 payload 与**同一份 FIT 里记录的值**一致 ⇒ 读坏/写坏/截断/位翻转会被抓到 | 防**故意篡改**：`value` 就存在同一个可改的 FIT 里，改完 payload 顺手改 `value` 照样 `+ OK` |
| 五个 payload 逐个校验（uboot / atf-1..3 / fdt） | **FIT 元数据不在哈希范围内**：`load`/`entry` 地址、`firmware`/`loadables` 指向、`ubuntuEnv.txt`、dtb overlay 都不校验（改 `load` 地址不会被发现） |
|  | **ATF 之后的一切**（内核 `vmlinuz`、`initrd.img`、`dtbs/`、`boot.scr`）都不在这套校验里 —— U-Boot 是**裸读**它们的。**idbloader 另有 BootROM 那一层的 sha256**（见本文下一节） |

`## Verified-boot: 0`（串口日志里紧跟 `Trying fit image at 0x4000 sector` 的那行）是
`fit_board_verify_required_sigs()` 打的（`arch/arm/mach-rockchip/fit_misc.c:210`，打印在 `:247`）。
**这个 0 是从 OTP 读出来的**，不是常量：SPL 分支里读 `OTP_SECURE_BOOT_ENABLE_ADDR` 那个字节，
`vboot = (vboot == 0xff)`（`fit_misc.c:213-227`）；本构建里这段**确实编进去了**
（`.config` 里 `CONFIG_SPL_ROCKCHIP_SECURE_OTP=y`），而且日志里没有
`Can't read verified-boot flag` ⇒ OTP 读到了、值不是 `0xff` ⇒ **Secure Boot 标志确实没烧**。
它的返回值还有一层作用：`fit_board_verify_required_sigs()` 是那个 weak 函数的 override，
返回 `1` 时 FIT 签名校验会变成**必需**（失败即停）。

本镜像的 FIT 里其实带了一个签名节点：

```dts
conf {
    firmware = "atf-1";
    loadables = "uboot", "atf-2", "atf-3";
    fdt = "fdt";
    signature { algo = "sha256,rsa2048"; key-name-hint = "dev";
                sign-images = "fdt", "firmware", "loadables"; };
};
```

但 **`CONFIG_SPL_FIT_SIGNATURE` / `CONFIG_FIT_SIGNATURE` 都没编**（实测
`build/u-boot-lubancat-rk3588/debian/build/lubancat-5-v2-rk3588/.config` 里两项都是
`# … is not set`），U-Boot 的控制 DTB 里也没有 `key-dev` 公钥 ⇒ **这一段在本工程里等于没有**。
⚠️ 所以**别把这套哈希当安全机制**：它只做完整性（防损坏），不做真实性（防篡改）。
下面这节说清"要防篡改得把哪几层都补上"。

## 想真正防篡改，得一整条链（只补中间一层没用）

| 级 | 谁验谁 | 现状 |
| --- | --- | --- |
| ① BootROM | **idbloader**（DDR 固件 + SPL） | **只验 sha256**：RKNS 头 `boot_flag=1`（sha256）、2 个 image 各 64 B 哈希槽、头部 512 B 哈希槽（实测都能重算对上，见下）。开 Secure Boot 时**同一槽位放 RSA 签名**，用 OTP 里烧的公钥哈希验 —— **没开** |
| ② SPL | **ATF（atf-1..3）+ U-Boot + 控制 DTB** | 只有 sha256 比对（见本文上一节）⇒ 防损坏。要防篡改需 `CONFIG_SPL_FIT_SIGNATURE` + 公钥进控制 DTB + 私钥重签 —— **没开** |
| ③ U-Boot proper | **内核** | **完全没有**：我们是 `boot.scr` 裸读 `/vmlinuz`、`/initrd.img`、`/dtbs/…`（要覆盖得改成"签名的内核 FIT"或 AVB） |
| ④ 内核/系统 | **rootfs** | 没有 dm-verity / AVB |

⛔ **只补 ② 不构成安全边界**：② 的前提是"① 里的 SPL 本身可信"。没开 Secure Boot 时，任何人能在
**maskrom 模式**（按住 MASKROM 键上电 / 短接，用 RKDevTool）重写一次 idbloader，就能把
**SPL + FIT 一起换成自己的**（哈希/签名自己生成，② 全过）⇒ 顺序不能颠倒：
**先有 ①（OTP 里的根信任），② 才有意义**。① 的代价：**eFuse 一次性、烧了不可逆**，还要自建
签名工具链 + 管好私钥（丢了就再也刷不进新固件）。所以"防篡改"其实是运维/生产决策，不只是配置项。

> 📄 OTP 的两个控制器、RK3588 的字段地图（`0x20` Secure Boot 标志 / `0x9c0` 公钥哈希 / `0x150`
> 防回滚）、SPL/BL31 与 Linux 各自的读取路径、为什么写不可逆，以及"BootROM 能不能反汇编"，
> 见 [`otp-secure-boot.md`](otp-secure-boot.md)。

idbloader 那层的实测（可自己重算）：

```bash
$ python3 - <<'EOF'
import hashlib
d=open('prebuilt/idbloader.img','rb').read()
so=int.from_bytes(d[120:124],'little'); size,off=so>>16,so&0xffff   # images[0]（DDR 固件）
print('boot_flag =', int.from_bytes(d[12:16],'little'), ' nimage =', int.from_bytes(d[8:12],'little')>>16)
print('image0.hash 记录/重算:', d[144:176].hex() == hashlib.sha256(d[off*512:off*512+size*512]).hexdigest())
print('header.hash 记录/重算:', d[1536:1568].hex() == hashlib.sha256(d[0:1536]).hexdigest())
EOF
boot_flag = 1  nimage = 2
image0.hash 记录/重算: True
header.hash 记录/重算: True
```

（结构见 `u-boot/tools/rkcommon.c` 的 `struct header0_info_v2`：`boot_flag[3:0]` 是哈希类型
`0=none/1=sha256/2=sha512`，`images[4]` 各有 64 B 哈希槽，末尾 512 B 是"头部自身的哈希或签名"。）

> 口径说明：**哈希槽里的值、`boot_flag`、以及"能重算对上"是我们直接从 `prebuilt/idbloader.img`
> 读出来核对的**；至于"BootROM 真的会去验它"，依据是 `tools/rkcommon.c` 里 `boot_flag[3:0]` 的
> 语义与 Rockchip 的设计约定（Secure Boot 时同一槽位放签名）——**不是**反汇编 BootROM 得到的结论：
> BootROM 是 SoC 内部掩膜 ROM，我们手里**根本没有它的二进制**（详见
> [`otp-secure-boot.md`](otp-secure-boot.md) §6）。

**自己动手核对（由弱到强）**：

1. **启动日志 vs 仓库 `prebuilt`**（最快）：把 `dumpimage -l prebuilt/u-boot.itb` 里的
   `Hash value` 与串口日志里 5 行 `## Checking <节点> … sha256(…) + OK` 的前缀逐一对照。
   `boot-serial-log.md` §4 那个"板上 U-Boot ≠ 仓库 prebuilt"的结论就是这么发现的。
2. **重算 payload 哈希**（证明 prebuilt 自身没坏）：⛔ **别把偏移写死** —— 每次重建 `u-boot.itb`，
   各 payload 的大小/偏移都会变（2026-10-02 这版就比上一版短 62 KB）。从 FIT 头读真值再算：
   ```bash
   # ① 抽出 FIT 头（文件前 fdt_totalsize 字节就是那棵 DTB），用 fdtget 读每个节点的真值
   TO=$(python3 -c "import struct;print(struct.unpack_from('>I',open('prebuilt/u-boot.itb','rb').read(),4)[0])")
   dd if=prebuilt/u-boot.itb of=/tmp/fit.dtb bs=1 count=$TO status=none
   for n in uboot atf-1 atf-2 atf-3 fdt; do
       echo "$n $(fdtget -t i /tmp/fit.dtb /images/$n data-offset) $(fdtget -t i /tmp/fit.dtb /images/$n data-size)"
   done
   # ② 按 base = FIT 基址 + FIT_ALIGN(fdt_totalsize)（本仓库这份 = 0x800）算 sha256，
   #    与 `dumpimage -l prebuilt/u-boot.itb` 里的 Hash value 逐个对照
   ```
   实测（2026-10-02 那版 prebuilt，base=0x800）：`uboot` `0x0`/1,158,832 B、`atf-1` `0x11b000`/143,216 B、
   `atf-2` `0x13e000`/36,864 B、`atf-3` `0x147000`/24,576 B、`fdt` `0x14d000`/13,304 B ——
   **5/5 与 `dumpimage -l` 记的哈希一致**（2026-10-03 复核）。
   ⚠️ 本文早先版本把这些偏移**写死在脚本里**（当时 `fdt` 在 `0x15c200`）⇒ 换一版 itb 就全错，已改成自推导。
   ⚠️ **别用 `dumpimage -p N -o …` 提取再算哈希** —— 本工程实测它对这种 external-data 的 FIT
   **取错窗口**（取出的 atf-1 比真实 payload 早约 3 KB），长度对、内容不对，会得到假 MISMATCH。
3. **从板上读回 bootloader 区与仓库逐字节比对**（唯一能证明"板上跑的就是这份"的办法）：
   ```bash
   SIZE=$(stat -c%s prebuilt/u-boot.itb)          # 2026-10-02 那版 = 1379328 → 2694 扇区
   sudo dd if=/dev/mmcblk0 of=/tmp/onboard.itb bs=512 skip=16384 count=$((SIZE/512))
   cmp -l /tmp/onboard.itb prebuilt/u-boot.itb | head    # 无输出 = 逐字节一致
   ```
   若不一致，先按 `boot-serial-log.md` §4 的办法判断板上那份到底是哪次构建的。

## 附录：只摘与结论有关的行

> 完整上下文按 `文件:行号` 查 `u-boot/`（下面每行都是从源码里原样摘的，Tab 折成 4 空格）。

| 结论 | 出处 | 关键几行 |
| --- | --- | --- |
| `## Checking <节点> … + OK` 是 SPL 打的 | `common/spl/spl_fit.c:250-273` | `printf("## Checking %s 0x%08lx ... ")` → `fit_image_verify_with_data()` → `puts("OK\n")` |
| 校验本体：算一遍、`memcmp`，不一致打 `Bad hash: <完整值>` 并返回 `-1` | `common/image-fit.c:1320-1356` | `calculate_hash(data, size, algo, value, &value_len)`；`memcmp(value, fit_value, value_len) != 0` → `printf(" Bad hash: ")` |
| 日志里那 10 个 hex = 现场算出哈希的**前 5 字节** | `common/image-fit.c:1355-1360` | `#ifdef CONFIG_SPL_BUILD` 下 `for (i = 0; i < 5; i++) printf("%02x", value[i]);` |
| external data 基址 = FIT 基址 + `FIT_ALIGN(fdt_totalsize)` | `common/image-fit.c:918-924` | `data_off += (ulong)fit + FIT_ALIGN(fdt_totalsize(fit));` |
| SPL 按 `data-offset` **从介质直读** payload | `common/spl/spl_fit.c:207-232` | `fit_image_get_data_offset(...)` → `offset += base_offset; external_data = true;` → 按扇区 `info->read(...)` |
| idbloader 的 RKNS 头结构 + 哈希怎么填 | `tools/rkcommon.c:66-75`、`:343-360` | `struct header0_info_v2 { … struct image_entry images[4]; … uint8_t hash[512]; };`；`hdr->boot_flag = cpu_to_le32(HASH_SHA256);`；每个子镜像 `do_sha256_hash(...)`，最后再对头部自身算一次 |
| `boot_flag[3:0]` 的取值 | `tools/rkcommon.c:29-33`、`:62` | `enum hash_type { … }` 与注释：`0:none, 1:sha256, 2:sha512` |
| `FIT: No boot partition` 从哪来 | `arch/arm/mach-rockchip/fit.c:154-157` | `part_get_info_by_name(dev_desc, part_name, &part) < 0` → `FIT_I("No %s partition\n", part_name)` |
