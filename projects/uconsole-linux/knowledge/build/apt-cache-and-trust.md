# APT 缓存复用与软件源信任

> 下载缓存只减少传输；真实性来自签名索引和包摘要，安装完成仍需独立检查。

## TL;DR

- 缓存 `.deb` 与缓存索引是不同策略；不要把旧索引当作当前源的真实性证据。
- 包按真实 Architecture 与摘要校验，文件名不能证明架构、版本或内容正确。
- 第三方源使用限定 keyring、Signed-By 与包级 pin，核对实际 Origin 和候选版本。
- 本仓库已有固定输入下载与 scoped 源；尚未实现跨主机 APT 缓存管理工具。

## 三层验证

1. APT 验证 InRelease/Release 签名与索引有效性。
2. 索引里的摘要验证下载包，缓存命中仍应经过 APT 的校验。
3. dpkg 配置、triggers 和依赖审计验证安装状态；下载成功不代表安装成功。

固定 Ubuntu Base tarball 与 pinctrl 源码另由 recipe 的 SHA-256 校验。
Base 固定不等于后续 Noble updates/security 的包版本冻结；严格复现还需要源快照和版本锁定。

## 缓存边界

只保存已下载 `.deb` 可以减少多轮传输；跨主机共享时 ARM64 与 all 包可由不同宿主复用，
但包依赖、发行版和目标 ABI 仍须匹配。不要因为宿主是 x86 就排除 all/arm64，
也不要因文件名包含 arm64 就跳过 `dpkg-deb -f`。

同名包保留已有文件是合并策略，不是内容相同的证明；损坏缓存应明确报错或受控重取。
缓存保存/恢复失败不能被收尾打印覆盖，未成功保存时应保留 staging 供恢复。
多个构建同时写同一缓存需要串行锁或原子发布，不能用互相覆盖文件实现共享。

## 第三方源与 pin

只读诊断命令，在目标系统或明确的隔离 chroot 执行：

```sh
apt-cache policy <PACKAGE>
apt-cache show <PACKAGE>
dpkg-deb -f <PACKAGE.deb> Package Version Architecture
apt-get -s upgrade
```

APT pin 的 release 字段必须匹配实际签名索引的字段；来源字符串写错可能不报错却失效。
检查模拟升级会选择哪个版本，而不仅检查 preferences 文件包含某个字符串。
第三方源的 keyring 不能无条件信任所有包；按需要限定包名和 priority。
不能用关闭签名验证或忽略过期索引来“修复”缓存。

## 安装与清理

需要未装依赖的本地 deb 用 APT 解析依赖；受控拆分内核配置流程另行说明。
安装后检查 `dpkg --audit`、`apt-get check` 和实际运行依赖；静态审计不执行离线目标程序。
清理下载缓存不删除 dpkg 数据库，清理临时代理/构建源不误删运行时需要的源。
不要把代理认证、Wi-Fi 密码或私有源 URL 放入公开 receipt。

## 本仓库依据

- Ubuntu recipe（工程源码：`image/ubuntu-base/recipe.json`）固定 Base 与 pinctrl 输入。
- Ubuntu 后端（工程源码：`scripts/image_backends/ubuntu_base.py`）的 download_verified 校验缓存与下载结果。
- [Ubuntu 应用策略](../desktop/desktop-applications.md)记录 Mozilla scoped trust 与包选择。
- [apt-secure(8)](https://manpages.ubuntu.com/manpages/noble/en/man8/apt-secure.8.html)和
  [apt_preferences(5)](https://manpages.ubuntu.com/manpages/noble/en/man5/apt_preferences.5.html)定义信任与 pin。
