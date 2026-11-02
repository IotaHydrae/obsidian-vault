# OTP / Secure Boot：本文引用的关键代码（原样摘录）

> 这是 [otp-secure-boot.md](otp-secure-boot.md) 的**代码附录**（原样摘录，一字未改）：
> U-Boot 的 OTP 字段宏与读取入口、`## Verified-boot: N` 的来源、防回滚计数、
> 烧 Secure Boot 字段的例程（⛔ **不要执行**）、安全 OTP 驱动为什么是"签入的生成汇编"、
> 内核读非安全 OTP 的寄存器级代码、`cpu-code`/`id` 的解释、以及 DT 里 OTP 控制器的写法。
> ⚠️ 这是一个**代码存档篇**（故意超过 300 行）：结论、字段地图、板上实测与"写入为什么危险"都在主篇。
> ⚠️ **§7.5 的例程是"看一眼知道长什么样"，⛔ 不要在任何板子上执行**（eFuse 一次写坏不可逆）。

## TL;DR

- 想改 OTP 相关行为（或判断"某个值是谁写的"）时来这里查**原样代码**；
  想先搞清"字段是什么、在哪、能不能写"看主篇 §1~§5。
- ⛔ 本附录里的烧写例程**仅供阅读**。


## 7. 附录：本文引用的关键代码（原样摘录）

> 摘录只保留与本文结论有关的行；行首 Tab 统一折成 4 空格。完整上下文按 `文件:行号` 查。

### 7.1 OTP 字段宏（U-Boot）

`u-boot/include/configs/rk3588_common.h:33-40`
```c
/* secure otp */
#define OTP_UBOOT_ROLLBACK_OFFSET	0x150
#define OTP_UBOOT_ROLLBACK_WORDS	2	/* 64 bits, 2 words */
#define OTP_ALL_ONES_NUM_BITS		32
#define OTP_SECURE_BOOT_ENABLE_ADDR	0x20
#define OTP_SECURE_BOOT_ENABLE_SIZE	1
#define OTP_RSA_HASH_ADDR		0x9c0
#define OTP_RSA_HASH_SIZE		32
```

### 7.2 读取的统一入口（U-Boot）

`u-boot/drivers/misc/misc_otp.c` —— `OTP_S` / `OTP_NS` 只是"能力位"，靠 `misc` 子系统选设备：
```c
struct udevice *misc_otp_get_device(u32 capability)
{
	return misc_get_device_by_capability(capability);
}

int misc_otp_read(struct udevice *dev, int offset, void *buf, int size)
{
	return misc_read(dev, offset, buf, size);
}

int misc_otp_write(struct udevice *dev, int offset, const void *buf, int size)
{
	return misc_write(dev, offset, (void *)buf, size);
}
```

### 7.3 `## Verified-boot: N` 的来源（U-Boot SPL）

`u-boot/arch/arm/mach-rockchip/fit_misc.c:210-249`
```c
/*
 * Override __weak fit_board_verify_required_sigs() for SPL & U-Boot proper.
 */
int fit_board_verify_required_sigs(void)
{
	uint8_t vboot = 0;

#ifdef CONFIG_SPL_BUILD
#if defined(CONFIG_SPL_ROCKCHIP_SECURE_OTP)
	struct udevice *dev;

	dev = misc_otp_get_device(OTP_S);
	if (!dev)
		return 1;

	if (misc_otp_read(dev, OTP_SECURE_BOOT_ENABLE_ADDR, &vboot, 1)) {
		printf("Can't read verified-boot flag\n");
		return 1;
	}

	vboot = (vboot == 0xff);
#endif
#else /* !CONFIG_SPL_BUILD */
#if defined(CONFIG_OPTEE_CLIENT)
	int ret;

	ret = trusty_read_vbootkey_enable_flag(&vboot);
	if (ret) {
		printf("Can't read verified-boot flag, ret=%d\n", ret);
		return 1;
	}
#elif defined(CONFIG_ROCKCHIP_PRELOADER_ATAGS)
	struct tag *t;

	t = atags_get_tag(ATAG_PUB_KEY);
	if (t && t->u.pub_key.flag == PUBKEY_FUSE_PROGRAMMED)
		vboot = 1;
#endif
#endif /* CONFIG_SPL_BUILD*/

	printf("## Verified-boot: %d\n", vboot);

	return vboot;
}
```

### 7.4 防回滚计数（U-Boot SPL）

`u-boot/arch/arm/mach-rockchip/spl.c:426-460` —— 读用 `fls()` 数已烧位；写只在"版本更靠前"时补烧：
```c
int fit_read_otp_rollback_index(uint32_t fit_index, uint32_t *otp_index)
{
	...
	dev = misc_otp_get_device(OTP_S);
	if (!dev)
		return -ENODEV;

	otp_version = 0;
	for (i = 0; i < OTP_UBOOT_ROLLBACK_WORDS; i++) {
		if (misc_otp_read(dev, OTP_UBOOT_ROLLBACK_OFFSET + i * 4,
		    &index,
		    4)) {
			printf("Can't read rollback index\n");
			return -EIO;
		}

		bit_count = fls(index);
		otp_version += bit_count;
	}
	*otp_index = otp_version;
	...
}

static int fit_write_otp_rollback_index(u32 fit_index)
{
	...
	dev = misc_otp_get_device(OTP_S);
	if (!dev)
		return -ENODEV;

	if (fit_read_otp_rollback_index(fit_index, &otp_index))
		return -EIO;

	if (otp_index < fit_index) {
		/* Write new SW version to otp */
		for (i = 0; i < OTP_UBOOT_ROLLBACK_WORDS; i++) {
		...
```

### 7.5 烧 Secure Boot 字段的例程（⛔ 不要执行）

`u-boot/lib/rsa/rsa-verify.c:594-619` —— 开头先看使能位，已烧就返回：
```c
int rsa_burn_key_hash(struct image_sign_info *info)
{
	...
	dev = misc_otp_get_device(OTP_S);
	if (!dev)
		return -ENODEV;

	ret = misc_otp_read(dev, OTP_SECURE_BOOT_ENABLE_ADDR,
			    &secure_flags, OTP_SECURE_BOOT_ENABLE_SIZE);
	if (ret)
		return ret;

	if (secure_flags == 0xff)
		return 0;

	sig_node = fdt_subnode_offset(blob, 0, FIT_SIG_NODENAME);
	if (sig_node < 0) {
		debug("%s: No signature node found\n", __func__);
		return -ENOENT;
	}
	...
```

`u-boot/lib/rsa/rsa-verify.c:677-710` —— 写公钥哈希（先拒绝重写）→ 回读校验 → 写 `0x20 = 0xff`：
```c
	/* burn key hash here */
	ret = misc_otp_read(dev, OTP_RSA_HASH_ADDR, digest_read, OTP_RSA_HASH_SIZE);
	if (ret)
		goto error;

	for (i = 0; i < OTP_RSA_HASH_SIZE; i++) {
		if (digest_read[i]) {
			printf("RSA: The secure region has been written.\n");
			ret = -EIO;
			goto error;
		}
	}

	ret = misc_otp_write(dev, OTP_RSA_HASH_ADDR, digest, OTP_RSA_HASH_SIZE);
	if (ret)
		goto error;

	memset(digest_read, 0, FIT_MAX_HASH_LEN);
	ret = misc_otp_read(dev, OTP_RSA_HASH_ADDR, digest_read, OTP_RSA_HASH_SIZE);
	if (ret)
		goto error;

	if (memcmp(digest, digest_read, digest_len) != 0) {
		printf("RSA: Write public key hash fail.\n");
		goto error;
	}

	secure_flags = 0xff;
	ret = misc_otp_write(dev, OTP_SECURE_BOOT_ENABLE_ADDR,
			     &secure_flags, OTP_SECURE_BOOT_ENABLE_SIZE);
	if (ret)
		goto error;

	printf("RSA: Write key hash successfully\n");
```

### 7.6 安全 OTP 驱动是"签入的生成汇编"

`u-boot/drivers/misc/rk3588-secure-otp.S:1-14` —— `.file` 指明了它的 C 源，但**树里没有那个 `.c`**：
```asm
	.arch armv8-a+nosimd
	.file	"rk3588-secure-otp.c"
	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.section	.text.secure_otp_ioctl,"ax",@progbits
	.align	2
	.type	secure_otp_ioctl, %function
secure_otp_ioctl:
.LFB274:
	.file 1 "drivers/misc/rk3588-secure-otp.c"
	.loc 1 232 0
	.cfi_startproc
```

`u-boot/drivers/misc/rk3588-secure-otp.S:96-112` —— 三个对外函数之一 `secure_otp_read` 的开头：
```asm
	.section	.text.secure_otp_read,"ax",@progbits
	.align	2
	.type	secure_otp_read, %function
secure_otp_read:
.LFB271:
	.loc 1 208 0
	.cfi_startproc
.LVL11:
	stp	x29, x30, [sp, -48]!
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	add	x29, sp, 0
	.cfi_def_cfa_register 29
```

### 7.7 内核读非安全 OTP（寄存器级）

`kernel-6.1/drivers/nvmem/rockchip-otp.c:85-95`：
```c
#define RK3588_OTPC_AUTO_CTRL		0x04
#define RK3588_OTPC_AUTO_EN		0x08
#define RK3588_OTPC_INT_ST		0x84
#define RK3588_OTPC_DOUT0		0x20
#define RK3588_NO_SECURE_OFFSET		0x300
#define RK3588_NBYTES			4
#define RK3588_BURST_NUM		1
#define RK3588_BURST_SHIFT		8
#define RK3588_ADDR_SHIFT		16
#define RK3588_AUTO_EN			BIT(0)
#define RK3588_RD_DONE			BIT(1)
```

`kernel-6.1/drivers/nvmem/rockchip-otp.c:523-570`：
```c
static int rk3588_otp_read(void *context, unsigned int offset, void *val,
			   size_t bytes)
{
	...
	addr_start = rounddown(offset, RK3588_NBYTES) / RK3588_NBYTES;
	addr_end = roundup(offset + bytes, RK3588_NBYTES) / RK3588_NBYTES;
	addr_offset = offset % RK3588_NBYTES;
	addr_len = addr_end - addr_start;
	addr_start += otp->data->ns_offset;

	...
	ret = clk_bulk_prepare_enable(otp->num_clks, otp->clks);
	...
	while (addr_len--) {
		writel((addr_start << RK3588_ADDR_SHIFT) |
		       (RK3588_BURST_NUM << RK3588_BURST_SHIFT),
		       otp->base + RK3588_OTPC_AUTO_CTRL);
		writel(RK3588_AUTO_EN, otp->base + RK3588_OTPC_AUTO_EN);
		ret = rk3588_otp_wait_status(otp, RK3588_RD_DONE);
		...
		out_value = readl(otp->base + RK3588_OTPC_DOUT0);
		memcpy(&buf[i], &out_value, RK3588_NBYTES);
		i += RK3588_NBYTES;
		addr_start++;
	}

	memcpy(val, buf + addr_offset, bytes);
```

`kernel-6.1/drivers/nvmem/rockchip-otp.c:740-748`（**`read_only = true`**）与 `:820-826`（**没有 `reg_write`**）：
```c
static struct nvmem_config otp_config = {
	.name = "rockchip-otp",
	.owner = THIS_MODULE,
	.read_only = true,
	.reg_read = rockchip_otp_read,
	.reg_write = rockchip_otp_write,
	.stride = 1,
	.word_size = 1,
};

static const struct rockchip_data rk3588_data = {
	.size = 0x400,
	.ns_offset = RK3588_NO_SECURE_OFFSET,
	.clocks = rk3588_otp_clocks,
	.num_clks = ARRAY_SIZE(rk3588_otp_clocks),
	.reg_read = rk3588_otp_read,
};
```

### 7.8 `cpu-code` / `id` 怎么被解释（内核）

`kernel-6.1/drivers/soc/rockchip/rockchip-cpuinfo.c:93-101`（大端两字节 → 数值）：
```c
		cell = nvmem_cell_get(dev, "cpu-code");
		if (!IS_ERR(cell)) {
			efuse_buf = nvmem_cell_read(cell, &len);
			nvmem_cell_put(cell);
			if (IS_ERR(efuse_buf))
				return PTR_ERR(efuse_buf);
			if (len == 2)
				rockchip_set_cpu((efuse_buf[0] << 8 | efuse_buf[1]));
			kfree(efuse_buf);
		}
```

`kernel-6.1/drivers/soc/rockchip/rockchip-cpuinfo.c:131-149`（`id` 两两交换 → CRC32 当 Serial）：
```c
	for (i = 0; i < 8; i++) {
		buf[i] = efuse_buf[1 + (i << 1)];
		buf[i + 8] = efuse_buf[i << 1];
	}
	...
	dev_info(dev, "SoC\t\t: %lx\n", rockchip_soc_id);

#ifdef CONFIG_NO_GKI
	system_serial_low = crc32(0, buf, 8);
	system_serial_high = crc32(system_serial_low, buf + 8, 8);

	dev_info(dev, "Serial\t\t: %08x%08x\n",
		 system_serial_high, system_serial_low);
#endif
```

### 7.9 DT：非安全 OTP 控制器与 cell 的写法

`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588s.dtsi:7548-7562`
```dts
	otp: otp@fecc0000 {
		compatible = "rockchip,rk3588-otp";
		reg = <0x0 0xfecc0000 0x0 0x400>;
		#address-cells = <1>;
		#size-cells = <1>;
		clocks = <&cru CLK_OTPC_NS>, <&cru PCLK_OTPC_NS>,
			 <&cru CLK_OTP_PHY_G>;
		clock-names = "otpc", "apb", "phy";
		resets = <&cru SRST_OTPC_NS>, <&cru SRST_P_OTPC_NS>,
			 <&cru SRST_OTPC_ARB>;
		reset-names = "otpc", "apb", "arb";

		/* Data cells */
		cpu_code: cpu-code@2 {
			reg = <0x02 0x2>;
		};
```

U-Boot 侧的 secure OTP 节点（`u-boot/arch/arm/dts/rk3588-u-boot.dtsi:20-24`）：
```dts
	secure-otp@fe3a0000 {
		u-boot,dm-spl;
		compatible = "rockchip,rk3588-secure-otp";
		reg = <0x0 0xfe3a0000 0x0 0x4000>;
	};
```
