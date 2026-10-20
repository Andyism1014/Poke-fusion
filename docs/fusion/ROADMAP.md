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
| 01 | 基础便利功能：IV/EV 精确显示、Box Link、快捷投球、DexNav、捕捉模式 | 低 | **完成** |
| 02 | Party+Box 统一素材选择器；遗传数据结构；存档容量审计与读写 | 高 | **完成**（见下） |
| 03 | 基础合宠：物种、IV、性格、招式遗传；炼妖终端道具；确认流程 | 中 | **完成**（见下） |
| 04 | 多被动战斗系统：跨种族主特性 + 3 个被动，先做 10 个代表性被动，再扩到 20~30 个 | 很高 | 未开始 |
| 05 | 永久三属性：继承、持久化、伤害/STAB、UI | 高 | **完成**（见下） |
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

## 阶段 02 已完成内容

| 功能 | 实现 | 位置 |
| --- | --- | --- |
| 遗传数据存储（方案 B） | 每只宝可梦的华丽大赛 6 项状态字节（48 bit）改为存放 `FusionGenetics`：主特性 9 + 被动 3×9 + 第三属性 5 + 世代 5 + flags 2。数据在加密区内，跟随宝可梦进出盒子、进化、交换，存档大小不变。华丽大赛状态值恒为 0（旧存档里原有的状态值会被误读成遗传数据，请新开档） | `include/pokemon.h`、`src/pokemon.c`、`src/fusion/fusion_save.c` |
| 队伍 + 盒子统一访问 | `FusionMonRef` 指向队伍或任意盒子的格子；读取父母时用临时副本；`Fusion_CommitFusion` 先检查全部条件，通过后才写入（子代占父母 A 的位置，删除父母 B，队伍自动整理，最后一只队伍成员不会被消耗） | `src/fusion/fusion_storage.c` |
| 炼妖终端（关键道具） | 开局获得。菜单：Appraise（鉴定）/ Storage（仓库）/ Exit；Fuse（合成）在阶段 03 加入 | `src/item_use.c`、`data/scripts/fusion.inc` |
| 素材选择器 | 两级列表：来源（队伍 + 14 个盒子，显示数量）→ 宝可梦（等级、IV 总和、被动数 P、第三属性标记 +T）；B 键逐级返回 | `src/fusion/fusion_ui.c` |
| 鉴定 | 显示特性、被动、第三属性、合宠世代 | `src/fusion/fusion_ui.c` |
| 调试菜单 | R+START → Fusion…：生成指定遗传数据的宝可梦、切换捕捉模式、填满当前盒子 / 全部盒子的随机素材 | `src/debug.c`、`src/fusion/fusion_debug.c` |

测试：`test/fusion/`（打包、加密、盒子移动、进化、队伍/盒子提交、迭代器、调试辅助）。

## 阶段 03 已完成内容

炼妖终端菜单：**Fuse** / Appraise / Storage / Exit。流程如下：

1. 选父母 1（保留它的格子）。
2. 选父母 2（会被消耗；列表里自动排除父母 1）。
3. 合法性检查。
4. 显示概率预览。预览只列出可能性和概率，确认后才掷骰，因此无法通过反复打开预览来刷结果。
5. 确认后生成子代，播放进化音乐，并显示子代的鉴定结果。

遗传规则（概率都在 `include/fusion/fusion_config.h`，逻辑在 `src/fusion/fusion_genetics.c`）：

| 项目 | 规则 |
| --- | --- |
| 物种 | 50% 父母 1、50% 父母 2 |
| 等级 | 取两者较低的 |
| 性格 | 50% / 50%（继承实际生效的性格，即薄荷修改后的） |
| IV | 每项 50% / 50%；另有 10% 概率额外 +1~3（上限 31） |
| 招式 | 双方招式去重后组成候选池，每个 50% 独立入选，最多 4 个；一个都没入选时，使用该物种的初始招式 |
| 主特性 | 50% / 50%，可以跨种族获得。是本物种的特性时，直接设置对应的特性槽；否则记为覆盖值 |
| 被动 | 候选池是双方的主特性和被动，去重并排除子代主特性；每个 45% 入选，超过 3 个时随机保留 3 个 |
| 第三属性 | 候选是双方的所有属性（物种属性 + 第三属性）中子代物种没有的；有候选时 50% 获得，从中随机取一个 |
| 世代 | 双方较高世代 + 1，上限 31 |
| 精灵球 / 持有物 | 精灵球来自提供物种的一方；子代持有父母 1 的持有物，父母 2 的持有物退回背包 |
| 禁止 | 两者是同一只、蛋、父母 2 是队伍最后一只而父母 1 在盒子里、任一方携带邮件、背包放不下父母 2 的持有物 |

注意：主特性覆盖值、被动、第三属性目前只会被记录和显示，要到阶段 04/05 才在战斗中生效。

测试：`test/fusion/genetics.c`，用 200 个种子批量验证分布、取值范围、去重、可复现性、跨种族覆盖、持有物退回、邮件拦截。
实机脚本：`dev_scripts/fusion_harness/scripts/terminal_fuse.txt`。截图：`docs/fusion/screenshots/phase03_fuse.png`。

## 阶段 05 已完成内容

第三属性在战斗中真正生效。

**实现方式**
- `struct BattlePokemon` 新增 `fusionType` 字段，表示永久第三属性，没有时为 `TYPE_MYSTERY`。
- 这个字段在 `PokemonToBattleMon`、战斗控制器的 `REQUEST_ALL_BATTLE` 中赋值。
- 原本把属性重置为"物种属性 + MYSTERY"的地方，第三个槽位改为写入 `fusionType`：开场、换入、形态变化、`RESTORE_BATTLER_TYPE`（Mimicry 还原用）。
- 字段位于 `pp` 之前，所以变身时会一并复制。

| 机制 | 行为 |
| --- | --- |
| 克制 / 免疫 | 3 个属性逐个相乘，可以出现 8 倍；与本身属性重复时不重复计算 |
| 本系加成 | 第三属性的招式也有 1.5 倍（Adaptability 下为 2 倍） |
| 羽栖（Roost） | 当回合去掉 Flying，包括第三属性上的 Flying |
| 浸水 / 魔法粉 | 变成单一属性，第三属性被清掉，换下后恢复。纯水系 + 第三属性的目标被浸水不会失败 |
| 森林诅咒 / 万圣夜 | 替换第三属性，换下后恢复 |
| 变身 | 复制第三属性 |
| 太晶化 | 覆盖全部属性 |
| AI | 选招时会考虑第三属性；换人时的属性匹配加入第三属性；STAB 推断计入第三属性；Illusion 伪装时不暴露真身的第三属性 |

**界面**
- 详情页显示第三个属性图标，队伍和盒子都支持。
- 战斗中 HP 条旁的属性图标改为常开（`B_SHOW_TYPES = SHOW_TYPES_ALWAYS`），最多显示 3 个。
- 截图：`docs/fusion/screenshots/phase05_third_type.png`。

调试菜单 Fusion… 新增 "Wild battle (own party)"：用自己的队伍和野生小拳石对战。实机脚本：`dev_scripts/fusion_harness/scripts/fused_wild_battle.txt`。

**测试**
- `test/fusion/battle_types.c`：12 个战斗测试。
- 测试框架新增 `FusionType(type)`，用法同 `TeraType`。

## 调研报告（阶段 02/04/05 的输入）

- [存档容量审计](research/save_audit.md)：`BoxPokemon` 只有 22 个空闲 bit，存档没有空闲 sector。
  已选定方案 B：挪用华丽大赛的状态位（48 bit），不改存档大小。
- [多特性战斗](research/multi_ability.md)：新增 `BattlerHasAbility`；在 5 类事件里加槽位游标；首批 10 个被动。
- [永久三属性](research/triple_type.md)：克制计算已支持 3 个槽位，主要工作是 `InitBattlerTypes`、Roost、AI 和 UI。

## 实机测试

`dev_scripts/fusion_harness/`：无界面 mGBA 脚本化测试（见其 README）。阶段 02 的截图在 `docs/fusion/screenshots/phase02_terminal.png`。

## 已知待决问题

- **中文显示**：游戏 `charmap.txt` 没有汉字，游戏内文本目前只能用英文。
  中文化需要额外做字库和字符映射，是一项独立的大工作，建议放在玩法稳定后。
- 特性冲突表（神奇守护、结实、魔法防守、天气/场地、特性无效化、变身/复制类）需在阶段 04 前定稿。
- 三属性与太晶化、变幻自如、魔法粉、万圣夜、森林诅咒的交互；第一版可禁止融合精灵太晶化。
