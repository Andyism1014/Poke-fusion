# Fusion 无界面实机测试工具

用 mGBA 内核（`libmgba-dev` 0.10）无界面运行 ROM，按脚本输入按键并截图。不需要显示器，速度约为实时的 30~40 倍。
首次运行时 `run.sh` 会用 apt 安装 `libmgba-dev libpng-dev`，并编译 `gbarun`。

```
./run.sh [--state FILE.ss0] [--elf FILE] [--sav FILE] [--rtc-epoch MS] ROM SCRIPT OUT_DIR
```

脚本语法：每行一条命令，`#` 开头为注释；多个按键用 `+` 组合。

| 命令 | 作用 |
| --- | --- |
| `wait N` | 空跑 N 帧 |
| `press KEYS [N=2] [AFTER=0]` | 按住 N 帧，松开后再等 AFTER 帧 |
| `hold KEYS N [AFTER]` | 同上，用于组合键 |
| `tap KEYS [COUNT] [GAP=10]` | 连按 COUNT 次，每次间隔 GAP 帧 |
| `shot NAME` | 截图到 `OUT_DIR/NAME.png` |
| `savestate NAME` / `loadstate NAME` | 保存 / 读取即时存档 |
| `read SYM`、`write SYM VALUE` | 读写内存（符号取自 ROM 旁边的 `.elf`） |

## 炼妖终端的回归流程

即时存档只对生成它的那一版 ROM 有效，所以 ROM 一重新编译就要从第 1 步重新做。

1. 开机 → Quickstart → 卡车里可以自由行动：
   `./run.sh pokeemerald.gba scripts/boot_to_overworld.txt out/boot`
   得到 `out/boot/overworld.ss0`。
2. 调试菜单 Set Party + 把 14 个盒子填满 420 只随机素材：
   `./run.sh --state out/boot/overworld.ss0 pokeemerald.gba scripts/setup_party_and_materials.txt out/setup`
   得到 `out/setup/mons.ss0`。
3. 依次测试终端的鉴定和仓库入口：
   `./run.sh --state out/setup/mons.ss0 pokeemerald.gba scripts/terminal_appraise.txt out/appraise`
   `./run.sh --state out/setup/mons.ss0 pokeemerald.gba scripts/terminal_storage.txt out/storage`

注意：调试菜单会记住上次的光标位置；列表滚动时，截图里的光标会比实际慢一帧刷新。
