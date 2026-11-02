# 登录失败延迟（PAM）：已去掉

> Debian/Ubuntu 默认认证失败要等 1.5~4.5 s 才让重输（**两处机制取较长者**）。
> 内网设备 + 串口反复试的场景下这很碍事，所以两处全部关掉。

## TL;DR

- 两处来源：`/etc/pam.d/login` 的 `pam_faildelay.so delay=3000000`（串口/本地控制台）与
  `common-auth` 里 `pam_unix.so` 缺 `nodelay`（SSH/GDM/`su`/`sudo` 都 include 它，~2 s）；
  libpam 的 `pam_fail_delay()` 取所有请求的**最大值**再加 ±50% 抖动。
- 修法：`pam_unix.so` 加 `nodelay` + 删 `/etc/pam.d/` 下所有 `pam_faildelay.so` 行 +
  同步改 `/usr/share/pam-configs/unix`（否则 `pam-auth-update` 会把它写回来）。
- 代价：**SSH 不再有失败节流**，口令强度只能靠你自己把关。
- 判据：`tools/verify-image.sh` 的「登录配置（PAM）」分组核对上述三处。

## 细节

Debian/Ubuntu 默认在认证失败后「先等一会儿再让你重输」（防暴力破解），一共有**两处**，按**较长者**
生效 —— libpam 的 `pam_fail_delay()` 取所有请求中的最大值，再乘 ±50% 随机抖动：

| 位置 | 作用范围 | 默认延迟 |
| --- | --- | --- |
| `/etc/pam.d/login` 的 `pam_faildelay.so delay=3000000` | 串口 / 本地控制台 | 基准 3 秒（实际 1.5~4.5 秒）|
| `common-auth` 的 `pam_unix.so` 缺 `nodelay` | SSH、GDM、`su`、`sudo` 都 include 它 | ~2 秒（`man pam_unix`）|

再叠加 `login.defs` 里的 `LOGIN_RETRIES=5`，串口上连续输错就成了「错一次等半天」。本机是内网设备、
串口/控制台本来就要反复试，所以**两处全部关掉**：`pam_unix.so` 加 `nodelay`、`/etc/pam.d/` 下所有
`pam_faildelay.so` 行删除；`/usr/share/pam-configs/unix` 用户档案也同步加了 `nodelay`，免得
`pam-auth-update` 重跑（装/升级 pam 模块包时的 dpkg trigger）又把延迟写回来。

代价与注意：
- **SSH 不再有失败节流**，口令强度完全靠你自己把关
- 以后自己装 pam 模块（指纹之类）时，`pam-auth-update` 可能判定「本地修改」而跳过，
  需要手工跑一次 `sudo pam-auth-update --force`
- `tools/verify-image.sh` 的「登录配置（PAM）」分组会核对上述三处
