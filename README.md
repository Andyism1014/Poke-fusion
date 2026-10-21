# 宝可梦：绿宝石·炼妖版 · Pokémon Emerald Fusion

把《梦幻西游》的"炼妖"带进宝可梦绿宝石：任意两只宝可梦都能炼成一只新的，招式、个体值、特性、被动和属性都会继承。基于 [RHH 的 `pokeemerald-expansion`](https://github.com/rh-hideout/pokeemerald-expansion)，保留完整的绿宝石剧情，第 1~9 世代的宝可梦都能出场。

*A Pokémon Emerald ROM hack built on RHH's pokeemerald-expansion: fuse any two Pokémon into a new one that inherits moves, IVs, abilities, passive abilities and types. The full Emerald story, Pokémon from all nine generations, and an endless post-game tower. [English summary below](#english).*

> [!IMPORTANT]
> 这是非官方的同人作品，与任天堂、Game Freak、Creatures 及 The Pokémon Company 无关。本仓库只包含源代码，不分发任何 ROM 文件。

![炼妖：预览卡与结果卡](docs/fusion/screenshots/readme_fusion.png)

## 特色

**炼妖**
- 用随身的 **Fusion Terminal**，从队伍或任意箱子里挑两只宝可梦炼成一只。
  - 炼之前会显示预览卡：物种、个体值、性格、特性、被动、招式和第三属性分别怎么继承、概率多少。
  - 子代占据父本 1 的位置，父本 2 被消耗掉。
- **1 个主特性 + 最多 3 个被动特性**，被动在战斗中和主特性一样生效。约 150 种常用特性已支持作为被动，对手 AI 也会考虑被动。
- **最多 3 个属性**：第三属性参与克制计算、属性一致加成和招式效果。
- **鉴定**：查看个体值、性格、特性、被动、属性和炼妖代数。

![摘要画面：SELECT 查看被动](docs/fusion/screenshots/readme_passives.png)

**捕捉与探索**
- **全世代遇敌表**：每个区域的野生宝可梦换成第 1~9 世代中强度相近、进化阶段相同的宝可梦。同一份存档里固定不变，每次新游戏重新随机。可以在设置里切回原版。
- 野生宝可梦偶尔自带被动或第三属性，抓到后会立即鉴定。
- 捕获模式：Classic（原版概率）/ Convenient（2 倍）/ Material（必定抓到，传说除外）。
- 开局就有 DexNav、随身电脑（Box Link）、跑鞋和全队学习装置。

![全世代遇敌表：DexNav 与野战](docs/fusion/screenshots/readme_wild.png)

**战斗**
- 选招时，招式名按对当前对手的效果着色：效果拔群为绿，效果不好为红，无效为灰。
- 属性图标常驻显示，第三属性一目了然。

![招式效果着色与炼妖塔对战](docs/fusion/screenshots/readme_battle.png)

**后期**
- **炼妖塔**：进入殿堂后解锁的无尽挑战，类似肉鸽玩法。
  - 对手随层数变强。
  - 层与层之间不回血，每赢一层三选一奖励，输一场即本轮结束。
- **碎片商店**：用塔里赚到的碎片购买被动水晶（随机被动）、属性棱镜（随机第三属性）和对战道具。
- **转生（New Game+）**：新周目重新随机世界，队伍回到 Lv.5 但保留个体值、招式和基因。之后每个周目对手更强。

![炼妖塔奖励与碎片商店](docs/fusion/screenshots/readme_endgame.png)

**体验优化**：默认快速文字，精确的 IV/EV 数值，招式回忆，驱虫剂续用提示，终端首次使用引导，摘要画面按 SELECT 查看被动。详见 [UX 审查记录](docs/fusion/UX_REVIEW.md)。

## 怎么玩

本仓库不提供 ROM。请按下一节自行编译出 `.gba` 文件，用任意 GBA 模拟器运行（推荐 [mGBA](https://mgba.io/)）。

- 游戏文本为英文。
- 新游戏开局，背包"重要物品"里就有 **Fusion Terminal**。

## 从源码编译

环境准备见 [`INSTALL.md`](INSTALL.md)（与上游 `pokeemerald-expansion` 相同）。

```sh
make -j$(nproc)            # 开发版 pokeemerald.gba，带调试菜单（R + START）
make release -j$(nproc)    # 发布版 pokeemerald-release.gba，无调试功能，推荐游玩用
make check -j$(nproc)      # 运行全部测试
```

> [!NOTE]
> 不要用 GitHub 的"Download ZIP"，它不包含提交历史，之后就无法合并上游更新。请用 `git clone`。

## 开发文档

- [`docs/fusion/ROADMAP.md`](docs/fusion/ROADMAP.md)：各阶段的设计与实现细节、存档格式（方案 B）、已知问题。
- [`docs/fusion/UX_REVIEW.md`](docs/fusion/UX_REVIEW.md)：体验审查与优化记录。
- [`docs/fusion/research/`](docs/fusion/research/)：被动特性、第三属性等调研。
- [`dev_scripts/fusion_harness/`](dev_scripts/fusion_harness/)：无界面的 mGBA 测试工具，用脚本按键、截图、存读状态。
- 主要代码：`src/fusion/`、`include/fusion/`、`data/scripts/fusion.inc`。测试：`test/fusion/`。
- 可调参数集中在 [`include/fusion/fusion_config.h`](include/fusion/fusion_config.h)：继承概率、野生基因概率、炼妖塔难度等。

## 反馈

这个改版的问题请提交到[本仓库的 Issues](https://github.com/Andyism1014/Poke-fusion/issues)。请不要报到上游：只有能在原版 `pokeemerald-expansion` 上复现的引擎问题，才按上游的 [`CONTRIBUTING.md`](CONTRIBUTING.md) 报给 RHH。

## 鸣谢

```
Based off RHH's pokeemerald-expansion 1.17.2 https://github.com/rh-hideout/pokeemerald-expansion/
```

- **[RHH (Rom Hacking Hideout)](https://github.com/rh-hideout/pokeemerald-expansion)** 的 `pokeemerald-expansion`：本作的引擎与绝大部分功能。所有贡献者名单见 [`CREDITS.md`](CREDITS.md)，引擎功能见 [`FEATURES.md`](FEATURES.md)，文档见 [rh-hideout.github.io/pokeemerald-expansion](https://rh-hideout.github.io/pokeemerald-expansion/)。
- **[pret](https://github.com/pret/pokeemerald)** 的 `pokeemerald` 反编译项目：`pokeemerald-expansion` 的基础。
- 宝可梦及相关名称、图像的版权归任天堂、Game Freak、Creatures 及 The Pokémon Company 所有。

---

## English

**Pokémon Emerald Fusion** is an unofficial Pokémon Emerald ROM hack built on [RHH's pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion). It adds a fusion system inspired by *Fantasy Westward Journey*.

- **Fusion Terminal**: fuse any two Pokémon from your party or any box.
  - The child inherits species, nature, IVs (with small mutations), moves, its main ability and passive abilities, and a third type.
  - A preview card shows how each part is inherited and with what odds before you confirm.
- **Up to 3 passive abilities** on top of the main ability, working in battle and known to the AI. **Up to 3 types.**
- **All-generation wild encounters**: every area gets randomized Gen 1–9 Pokémon of similar strength, fixed per save and re-rolled for each new game. Wild Pokémon can carry passives or a third type, and are appraised as soon as you catch them.
- Capture modes, DexNav, a portable PC, Exp. Share and running shoes from the start.
- Move names are coloured by effectiveness against the target, and type icons are always shown.
- **Post-game**:
  - An endless roguelike **Fusion Tower**.
  - A **Shard Shop** selling Passive Crystals, Type Prisms and battle items.
  - **Reincarnation (New Game+)**: keep your party's genes at Lv. 5 in a freshly rolled world, with stronger trainers each cycle.

No ROMs are distributed. Build it yourself (see [`INSTALL.md`](INSTALL.md)): `make release` produces `pokeemerald-release.gba`, which you can play in any GBA emulator. Game text is in English.

Please report bugs in this hack to [this repository's issues](https://github.com/Andyism1014/Poke-fusion/issues), not upstream.

Based off RHH's pokeemerald-expansion 1.17.2 https://github.com/rh-hideout/pokeemerald-expansion/. Please also see [`CREDITS.md`](CREDITS.md) for everyone who contributed to the engine. Pokémon is © Nintendo, Game Freak, Creatures and The Pokémon Company. This is a non-commercial fan project.
