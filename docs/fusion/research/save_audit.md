# 阶段 02 调研：存档容量审计（FusionGenetics）

> 基线 `f4dc700a`。数据来自源码和构建产物（`pokeemerald.elf`/`.map`、nm、objdump）。

## 需求位数

| 字段 | bits | 依据 |
|---|---|---|
| 主特性 + 3 被动 | 4 × 9 = 36 | `ABILITIES_COUNT` = 320（include/constants/abilities.h:344） |
| 第三属性 | 5 | `NUMBER_OF_MON_TYPES` = 21（include/constants/pokemon.h） |
| 合宠世代 | 4~5 | |
| flags | ~3 | |
| **合计** | **~50** | |

## 1. 结构体大小与空闲 bit

- `NUM_SUBSTRUCT_BYTES` 取 4 个 substruct 的最大 sizeof（include/pokemon.h:236），4 个都是 12 字节，`secure` 共 48 字节，必须是 4 的倍数。
- `sizeof(struct BoxPokemon)` = 80（32 字节头 + 48 字节 secure），`sizeof(struct Pokemon)` = 100（`gParties` = 0x960 = 24 × 100）。

| 字段 | 结构 | 行 | bits |
|---|---|---|---|
| unused_02 | Substruct0 | pokemon.h:137 | 6 |
| unused_04 | Substruct0 | :140 | 3 |
| unused_0A | Substruct0 | :145 | 2 |
| unused_04 | Substruct1 | :155 | 5 |
| unused_06 | Substruct1 | :157 | 3 |
| unused_0B | Substruct3 | :220 | 1 |
| unused_13 | BoxPokemon（不加密） | :270 | 1 |
| unused_1E | BoxPokemon（不加密） | :277 | 1 |

**合计 22 bits**，其中 20 bits 在加密 + checksum 区。

可以挪用但要砍功能的 bit：`blockBoxRS` 1；不再发放的 marine/land/sky 缎带 3、event 缎带 4、isShadow 1；
不用极巨化时 `dynamaxLevel` 4 + `gigantamaxFactor` 1；**不用华丽大赛时 Substruct2 的 cool..sheen 共 48 bits**；
昵称第 11/12 字 16 bits（要把 `POKEMON_NAME_LENGTH` 改回 10）。

## 2. 存档 sector 布局

每个 sector 3968 字节数据 + 116 字节 SB3 chunk。每个 slot 14 个 sector，共 2 个 slot。
SB2 用 sector 0，SB1 用 1-4，Storage 用 5-13；28-29 是名人堂，30 是 Trainer Hill，31 是战斗录像，**没有空闲 sector**。

| Block | 实际大小 | 容量 | 剩余 |
|---|---|---|---|
| PokemonStorage | 34144 | 9 × 3968 = 35712 | **1568** |
| SaveBlock1 | 15568 | 15872 | 304 |
| SaveBlock2 | 3884 | 3968 | 84 |
| SaveBlock3 | 4 | 1624 | 1620 |
| RecordedBattleSave | 4092 | 4092 | **0** |

PokemonStorage = 4 + 420 × 80 + 126 + 14 + `fusions[4]` × 100（pokemon_storage_system.h）。
有 STATIC_ASSERT 守卫：save.c:80-83、recorded_battle.c:34、hall_of_fame.c:41。
宝可梦还会存在：SB1 `playerParty[6]`、`daycare` 的 2 只 BoxPokemon、`RecordedBattleSave.parties[4][6]`。

## 3. 两种方案

### (a) BoxPokemon 加 4 字节（追加在 `secure` 之后）

- 加在 substruct 里不划算：union 按最大 substruct 补齐，任何一个 substruct 加 4 字节，BoxPokemon 就要加 16 字节。
- 加密、解密和 checksum 只按 `ARRAY_COUNT(secure.raw)` 遍历（pokemon.c:1339-1370, 1892-1907），所以 secure 后面追加的 tail 是安全的。但 tail 不加密，除非把它算进 checksum。
- 代码里没有写死 80/100，都用 sizeof。联机 `BLOCK_BUFFER_SIZE` 0x100，每次传 2 只，2 × 104 字节也装得下。
- **+4 字节就是上限**：Storage 增加 1696 字节，超出剩余 1568 字节，溢出 128；战斗录像溢出 96。因此必须同时：
  - 把 `fusions[]` 移到 SaveBlock3（需要 416 字节，有 1620）。之后 Storage 剩 288 字节。
  - 把 `BATTLER_RECORD_SIZE` 从 388 降到 364（recorded_battle.h:8）。影响只是很长的战斗录不全。
- EWRAM 净增约 2.1 KB（86% → 约 87%）。
- 旧存档全部失效；与原版/其他版本联机交换不兼容；改完后 Storage 和 SB1 各只剩约 280 字节余量。

推荐布局：

```c
// Substruct0 重排（仍为 12B，在加密区）
u16 species:11, teraType:5;
u16 heldItem:10, pokeball:6;
u32 experience:21, fusionAbility:9, fusionFlags:2;
u8 ppBonuses, friendship, nickname11, nickname12;
// Substruct1
u16 move3:11, fusionType3:5;                                  // 替换 unused_04
u16 move4:11, fusionFlags2:3, hyperTrainedHP:1, hyperTrainedAttack:1; // 替换 unused_06
// BoxPokemon 在 secure 之后追加
struct FusionGenetics { u32 passive1:9, passive2:9, passive3:9, generation:5; } fusion;
```

### (b) 不改大小，只打包

22 个空闲 bit + 挪用约 28 bit。可行组合只有两种：
- **去掉华丽大赛的 5 项状态 + 光泽（48 bits）**；
- 去掉极巨化 + event 缎带 + 12 字符昵称（29 bits）。

好处：存档大小不变、旧存档兼容（全 0 = 无融合数据）、数据全部在加密区。
代价：失去对应的游戏功能。

（备选：把 sector 30/31 改成独立存储区。没有双缓冲，和主存档也不是原子写入，**不推荐**。）

## 需要改动的文件

- include/pokemon.h：字段、`MON_DATA_FUSION_*`、`STATIC_ASSERT(sizeof(struct BoxPokemon) == ...)`
- src/pokemon.c：`sBoxPokemonConstantsFit`、`GetBoxMonData3`/`SetBoxMonData`，可能还有 checksum
- 方案 (a) 另需：include/pokemon_storage_system.h、include/global.h（`fusions` 移到 SB3）、src/party_menu.c 中的引用、include/recorded_battle.h
