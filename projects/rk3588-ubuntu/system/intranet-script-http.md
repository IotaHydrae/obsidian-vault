# 内网脚本分发：发行版 nginx 的 docroot 放现场脚本（可照抄到别的机器）

> **干什么用的**：让现场/工程师在**任意一台内网机器**上一条命令就把脚本拿到并跑，
> 不用 ssh 传、不用 U 盘、不用记路径。
> 当前落地：编译服务器 **`<HOST_05>`**（`example-projectDell`）的 `/var/www/html`，走发行版 nginx，端口 80。
>
> ⛔ **不要**再用"自己写 systemd 单元 + 自定义端口（如 8899）+ `systemd --user`/linger"那套
> （2026-09-29 用户明确"不要搞一堆 board 的东西"）—— 标准做法就是：**发行版 nginx + docroot**。

---

## TL;DR

- 用途：让现场在**任意一台内网机器**上一条命令拿到并运行修复脚本（我们托管在编译服务器的 nginx docroot 里）。
- **标准做法 = 发行版 nginx 的 docroot**（不起自定义服务）：装 nginx → `tools/lsp-fix-http.sh publish` → 现场 curl。
- ⛔ 现场**别写 `curl … | sudo bash`**（sudo 要密码时会从管道把脚本读走）；
  要 `curl -o /tmp/fix.sh && sudo bash /tmp/fix.sh`，且**必须 bash 不能 sh**。
- 照抄到别的机器的步骤见 §1；`publish` 放了什么见 §2；排障与回退见 §5/§6。

## 1. 在别的机器上怎么配（照着抄，一次搞定）

```bash
# ① 装 nginx（Debian/Ubuntu 的默认站点就是 80 + /var/www/html，装完 enabled ⇒ 开机自启）
sudo apt-get install -y nginx-light
systemctl is-enabled nginx            # → enabled
curl -fsS -o /dev/null -w '%{http_code}\n' http://127.0.0.1/     # → 200

# ② 把这台机器变成"分发机"：把本仓库的源码同步过来（只要 tools/ 与 tests/，同步脚本会排除 build/ images/）
./tools/sync-to-board.sh -H <user@这台机器>

# ③ 发布脚本进 docroot（默认 LSP_FIX_HTTP_DIR=/var/www/html、LSP_FIX_HTTP_PORT=80；幂等，可反复跑）
sudo /path/to/image-project/tools/lsp-fix-http.sh publish

# ④ 自检
curl -fsS -o /dev/null -w '%{http_code}\n' http://127.0.0.1/fix-logind-no-removeipc.sh   # → 200
curl -fsS http://127.0.0.1/SHA256SUMS
```

可选（都不影响功能）：

- **换目录/端口**：`LSP_FIX_HTTP_DIR=/srv/www LSP_FIX_HTTP_PORT=8899 …` —— ⚠️ 换端口得自己加 nginx 的
  `server { listen 8899; root …; }`（默认站点只管 80）；**不推荐**，80 端口最省事、现场 URL 也最短。
- **防火墙**：`ufw status`；如果是 active，要 `sudo ufw allow 80/tcp`（我们的机器上 ufw 是 inactive）。
- **根目录索引页**（可选，浏览器点着方便）：把下面存成 `/var/www/html/index.html`，
  再 `sudo rm -f /var/www/html/index.nginx-debian.html`（那只是 nginx 的欢迎页）；
  ⚠️ 这一步是**手工**的，`publish` **不会**动 docroot 里别的文件：

  ```html
  <!doctype html><meta charset=utf-8><title>现场脚本</title>
  <h3>现场脚本（内网 HTTP）</h3>
  <ul>
  <li><a href=fix-logind-no-removeipc.sh>fix-logind-no-removeipc.sh</a> —— 老镜像补 RemoveIPC=no（+ linger）</li>
  <li><a href=verify.sh>verify.sh</a> —— 验证修复有没有生效（金丝雀法，不故障注入）</li>
  <li><a href=repro.sh>repro.sh</a> + <a href=dds-link-monitor.py>dds-link-monitor.py</a> —— 复现“六路全断”（工程师用）</li>
  <li><a href=SHA256SUMS>SHA256SUMS</a> · <a href=README.txt>README.txt</a></li>
  </ul>
  ```

## 2. `publish` 往 docroot 里放什么（幂等；root:root 644/755）

| 文件 | 内容 |
| --- | --- |
| `fix-logind-no-removeipc.sh` + `.sha256` | 老镜像修复（logind 的 `RemoveIPC=no` + linger），见 [collection-dds-shm-fault.md](collection-dds-shm-fault.md) |
| `fix.sh` | **符号链接** → `fix-logind-no-removeipc.sh`（旧链接兼容） |
| `verify.sh` | 验证修复是否真的生效（金丝雀法，不故障注入） |
| `dds-shm-status.sh` | 看 DDS 共享内存的**持有情况**（只读）：段/信号量数量、按 UID 计数、每个进程的 maps 与 `(deleted)` 数、SysV 概况、判定行 |
| `repro.sh` + `dds-link-monitor.py` | 复现"六路全断"的脚本 + 监视器（工程师用；**故意制造故障**、会断 ssh） |
| `SHA256SUMS` | 上面三个脚本的 sha256（`sha256sum -c --ignore-missing SHA256SUMS`） |
| `README.txt` | 现场命令（**按发布时本机 IP 自动生成**，所以换机器后 URL 会跟着变） |

源都在本仓库：`tools/lsp-fix-logind-no-removeipc.sh`、`tools/lsp-verify-removeipc.sh`、
`tests/dds-shm-fault-repro.sh`、`tests/dds-link-monitor.py`；发布器是 `tools/lsp-fix-http.sh`。

## 3. 现场怎么用（完整命令，没有变量）

脚本必须用 **`bash`**（里面有 `read -d`/数组，`sh` 会报 `read: Illegal option -d`）。
把 `<HOST_05>` 换成你那张"分发机"的 IP：

```bash
# ① 修复（推荐）
curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh -o /tmp/fix.sh && sudo bash /tmp/fix.sh
# ② 修复 + sha256 校验
cd /tmp && curl -fsSLO http://host-05.example.com/fix-logind-no-removeipc.sh && curl -fsSLO http://host-05.example.com/fix-logind-no-removeipc.sh.sha256 && sha256sum -c fix-logind-no-removeipc.sh.sha256 && sudo bash fix-logind-no-removeipc.sh
# ③ 只看会做什么（不落盘、不重启 logind）
curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh -o /tmp/fix.sh && sudo bash /tmp/fix.sh --dry-run
# ④ 只体检（只读、不用 root）
curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh -o /tmp/fix.sh && bash /tmp/fix.sh --check
# ⑤ 验证修复有没有生效（不故障注入、不碰采集栈）
curl -fsSL http://host-05.example.com/verify.sh -o /tmp/verify.sh && sudo bash /tmp/verify.sh
# ⑤b 看 DDS 共享内存持有情况（只读；建议 sudo）
curl -fsSL http://host-05.example.com/dds-shm-status.sh -o /tmp/dds-shm-status.sh && sudo bash /tmp/dds-shm-status.sh
# ⑥ 复现脚本（工程师用；会断 ssh、会把 LCD 打到"六路全断"）
cd ~ && curl -fsSLO http://host-05.example.com/repro.sh && curl -fsSLO http://host-05.example.com/dds-link-monitor.py && chmod +x repro.sh dds-link-monitor.py && ./repro.sh --dry-run
# ⑦ 旧名字（符号链接，仍可用）
curl -fsSL http://host-05.example.com/fix.sh -o /tmp/fix.sh && sudo bash /tmp/fix.sh
```

⛔ **不要写 `curl … | sudo bash`**：sudo 需要密码时**会从管道里读密码**，把脚本当密码吃掉
（板上实测 `sudo: 3 incorrect password attempts`、脚本一行没跑）。非要用管道就先缓存凭据：

```bash
sudo -v && curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh | sudo bash
```

## 4. 更新脚本的流程

```bash
# 1) 在开发机改仓库里的脚本（tools/lsp-*.sh、tests/dds-*.sh）
# 2) 同步到"分发机"
./tools/sync-to-board.sh -H <user@分发机>
# 3) 在分发机上重新发布（nginx 立刻生效，不用 reload/重启任何东西）
sudo /path/to/image-project/tools/lsp-fix-http.sh publish
```

## 5. 核对/排障（在新机器上照着跑）

```bash
dpkg -V nginx-common nginx-light        # 空 = 包自带的文件都没被改过（我们的配置一行没动）
nginx -T | grep -nE 'listen|root'       # 生效配置：默认站点 80 + /var/www/html
ls -l /etc/nginx/sites-enabled/         # 只有 default
ss -ltnp | grep ':80 '                  # 谁在听 80（应该是 nginx）
ufw status                              # enabled 的话要 allow 80/tcp
ls -l /var/www/html/                    # 发布出来的文件（root:root，644/755）
curl -fsS -o /dev/null -w '%{http_code}\n' http://127.0.0.1/fix-logind-no-removeipc.sh
```

## 6. 回退

```bash
sudo apt-get purge -y nginx-light nginx-common nginx libnginx-mod-http-echo
rm -f /var/www/html/{fix-logind-no-removeipc.sh,fix-logind-no-removeipc.sh.sha256,fix.sh,verify.sh,repro.sh,dds-link-monitor.py,SHA256SUMS,README.txt,index.html}
```

（`purge nginx-common` 会连 `/etc/nginx` 和空的 docroot 一起删；nginx 自带的欢迎页属于包文件，
重装就回来。）

## 7. 坑

- ⛔ 脚本要 **bash**，别用 `sh`；⛔ 别 `curl | sudo bash`（见 §3）。
- docroot 是**内网谁都能读**的：只放"现场可公开"的脚本/说明，**别放密钥**。
  （目前这几个文件里没有密钥；维护密码、部署 token 都不在这里。）
- `README.txt` 里的 URL 是**发布时按本机 IP 生成的**：换了分发机（或换网段）要重新 `publish`。
- 这台"分发机"就专职分发，别在它上面跑采集/编译重活，也别给它加自定义服务。
- 现场机器如果**跨网段**，先在现场机 `curl` 一下确认通（我们实测 `<HOST_05>` 从
  `10.128.41.x`/`10.128.43.x` 两个网段都通）。

## 8. 已配置的实例（2026-09-29，供对照）

- **服务器 `<HOST_05>`（`example-projectDell`）**：`apt-get install -y nginx-light`
  （2026-09-29 12:34:53，装进 `nginx-light`/`nginx`/`nginx-common`/`libnginx-mod-http-echo`
  1.24.0-2ubuntu7.18）；docroot `/var/www/html`；`nginx.service` = enabled + active（开机自启）；
  `dpkg -V` 干净、`ufw` inactive、**没有任何自定义 systemd 单元**；文件由
  `sudo /path/to/image-project/tools/lsp-fix-http.sh publish` 发布。
  另外手工加了根目录 `index.html`（§1 可选步骤）并删掉了 nginx 欢迎页。
- **开发机 `<HOST_06>`**：2026-09-29 试过 `systemd --user` + 端口 8899 —— **已停用、单元已删、
  目录已删**；不要再回到那种方式。
