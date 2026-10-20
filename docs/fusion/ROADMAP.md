# 《宝可梦：绿宝石·炼妖版》V2.0 开发路线

基于 `rh-hideout/pokeemerald-expansion`（基线提交 `f4dc700a`）。

核心目标：**自由合宠 + 多被动特性 + 多属性 + 全局宝可梦管理 + 高效率抓宠**。
V2.0 范围：4 个同时生效的特性（1 主 + 3 被动）、最多 3 种永久属性、任意队伍/盒子随时合宠。
超过 4 个招式、4+ 属性、变异系统放到后续版本。

## 设计原则

1. 合宠的核心是遗传自由度：招式、被动、属性均可跨种族。限制只来自机制兼容、槽位、概率和平衡。
2. 尽量减少重复劳动：抓到即鉴定、自动入箱、随时筛选与合宠。
3. 先保证战斗与存档稳定，再追求更多组合。

## 阶段

| 阶段 | 内容 | 难度 | 状态 |
| --- | --- | --- | --- |
| 01 | 基础便利功能：IV/EV 精确显示、Box Link、快捷投球、DexNav、捕捉模式 | 低 | **进行中**（见下） |
| 02 | Party+Box 统一素材选择器；遗传数据结构；存档容量审计与读写 | 高 | 未开始（已做初步审计，见下） |
| 03 | 基础合宠：物种、IV、性格、招式遗传；炼妖终端道具；确认流程 | 中 | 未开始 |
| 04 | 多被动战斗系统：跨种族主特性 + 3 个被动，先做 10 个代表性被动，再扩到 20~30 个 | 很高 | 未开始 |
| 05 | 永久三属性：继承、持久化、伤害/STAB、UI | 高 | 未开始 |
| 06 | 体验打磨：素材筛选、自动标记、捕捉后鉴定、概率提示、统计 | 中 | 未开始 |

每个阶段的验收要求：独立 commit；`make` 通过；`make check` 相关测试通过；
mGBA 实测关键操作；测试存读档、进化、仓库移动。

## 阶段 01 已完成内容

| 功能 | 实现 | 位置 |
| --- | --- | --- |
| IV/EV 精确数值 | 开启 `P_SUMMARY_SCREEN_IV_EV_INFO/VALUES/TILESET`。在详情页能力页按 A 在 能力 → IV → EV 间切换，队伍和盒子里都可以 | `include/config/summary_screen.h` |
| 随身 PC | 新游戏直接获得 `Pokémon Box Link` 关键道具（可注册到 SELECT） | `src/new_game.c` |
| DexNav | 启用并分配 flag/var；新游戏即出现在开始菜单；野外按 R 快速搜索已注册种族 | `include/config/dexnav.h`、`include/constants/flags.h`、`include/constants/vars.h` |
| 快捷投球 | Expansion 默认已开启：战斗中按 R 投上次用的球，按住 R + 方向键切换球种 | `include/config/battle.h`（`B_LAST_USED_BALL`） |
| 捕捉模式 | 经典（原版）/ 便捷（普通野生 ×2，默认）/ 素材（普通野生必定捕获）。传说、幻之、究极异兽不受影响。运行时由 `VAR_FUSION_CAPTURE_MODE` 控制，不需要重新编译 | `include/fusion/fusion_config.h`、`src/fusion/fusion_capture.c` |

捕捉模式目前只能通过脚本/调试菜单修改变量，切换入口会放进阶段 03 的「炼妖终端」菜单。

测试：`test/fusion/capture.c`（捕捉倍率、传说豁免、越界回退、新游戏初始化）。

## 开发约定

- 所有炼妖代码放在 `src/fusion/`、`include/fusion/`、`test/fusion/`；Makefile 会自动收录子目录。
- 对原版文件只做最小的挂钩改动（一行调用 + include），逻辑写在 fusion 模块里。
- 可调参数集中在 `include/fusion/fusion_config.h`。
- 新增 flag/var 在 `include/constants/flags.h`、`vars.h` 中把 `*_UNUSED_*` 改名占用，并注明 `Fusion:`。
- 构建：`make -j$(nproc)`；测试：`make check -j$(nproc) TESTS="Fusion"`，全量 `make check -j$(nproc)`。

## 调研报告（阶段 02/04/05 的输入）

- [存档容量审计](research/save_audit.md)：`BoxPokemon` 只有 22 个空闲 bit，存档没有空闲 sector。
  两条路线：(a) `BoxPokemon` +4 字节（旧存档失效，要把 `fusions[]` 移到 SaveBlock3，并缩小战斗录像）；
  (b) 不改大小，挪用华丽大赛的状态位（48 bit）。**需要拍板。**
- [多特性战斗](research/multi_ability.md)：新增 `BattlerHasAbility`；在 5 类事件里加槽位游标；首批 10 个被动。
- [永久三属性](research/triple_type.md)：克制计算已支持 3 个槽位，主要工作是 `InitBattlerTypes`、Roost、AI 和 UI。

## 已知待决问题

- **中文显示**：游戏 `charmap.txt` 没有汉字，游戏内文本目前只能用英文。
  中文化需要额外做字库和字符映射，是一项独立的大工作，建议放在玩法稳定后。
- 特性冲突表（神奇守护、结实、魔法防守、天气/场地、特性无效化、变身/复制类）需在阶段 04 前定稿。
- 三属性与太晶化、变幻自如、魔法粉、万圣夜、森林诅咒的交互；第一版可禁止融合精灵太晶化。
