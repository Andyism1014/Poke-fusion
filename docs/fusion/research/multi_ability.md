# 阶段 04 调研：多特性（1 主特性 + 3 被动）

> 基线 `f4dc700a`。行号是调研时的位置，实施前需要重新核对。

## 现状

- 每个战斗位只有一个 `struct BattlePokemon.ability`（include/pokemon.h:357）。压制类状态放在 `volatiles`：`gastroAcid`、`overwrittenAbility`、`neutralizingGas`、`traceActivated`。
- `.ability` 有三个初始化点：battle_main.c:3140（开场）、battle_script_commands.c:3243（`Cmd_switchindataupdate`）、battle_util.c:9581（`CopyMonAbilityAndTypesToBattleMon`）。
- 中心访问器 `GetBattlerAbilityInternal`（battle_util.c:4928）统一处理不可压制、Gastro Acid、Neutralizing Gas、Mold Breaker 和 Ability Shield。`CanBreakThroughAbility`（:4906）固定读 `.ability`，需要改。
- 事件总入口 `AbilityBattleEffects(caseID, battler, ability, move, trigger)`（:3045）**已经把 ability 作为参数传入**，这是最大的有利条件。
- 没有数据驱动的分发表。`AbilityInfo`（pokemon.h:567）带 `cantBeCopied/Swapped/Traced/Suppressed/Overwritten/breakable` 标志。

| 调用点（粗略统计） | 数量 |
|---|---|
| `GetBattlerAbility` | ~238 |
| `== ABILITY_` / `!= ABILITY_` | 175 / 62 |
| `case ABILITY_` | 495 |
| 伤害计算 ctx 里的 `abilities[]` | ~309 |
| AI 的 `abilities[]` | ~356 |
| `IsAbilityAndRecord` / `IsAbilityOnSide` / `IsAbilityOnField` | 45 / 37 / 10 |

## 统一判断入口

仓库里还没有 `BattlerHasAbility`。需要新增：

- `GetBattlerAbilitySlot(b, slot, ...)`：按单个特性做压制和破特性判断。
- `BattlerHasAbility(b, X)`，以及 `IgnoreMoldBreaker`、`AndRecord` 两个变体。
- `GetBattlerActiveAbilities(b, out[4])`：返回时去重，顺序为主特性、被动 1、被动 2、被动 3。

其余做法：

- `GetBattlerAbility` 仍然只返回主特性，保证旧代码能编译。
- `IsAbilityOnSide/Field`、`Cmd_jumpifability`、`IsAbilityPreventingEscape` 内部改用新函数，顺带修好约 50 处调用。
- 带 `enum Ability` 参数的函数用宏 `ABILITY_IS(battler, param, X)` 机械替换：`param == X || BattlerHasActivePassive(battler, X)`。
- 存储：在 `BattlePokemon` 的 `ability` 后、`pp` 前加 `enum Ability passives[3]`。Transform 按 `offsetof(pp)` 做 memcpy，会自动复制被动。测试框架加 `Passives(a,b,c)`。

## 事件触发

所有事件都是可重入状态机，所以要在 `struct EventStates`（include/battle.h:565）加 `abilitySlot:2` 游标。4 个槽都处理完，外层状态才往下推进。

- 入场：battle_switch_in.c:46-146
- 回合末：battle_end_turn.c:131-164、410-425、1330-1345
- 命中后：battle_move_resolution.c:3424-3478
- 免疫/状态治愈：`TryImmunityAbilityHealStatus`
- 离场：`Cmd_switchoutabilities`。Natural Cure 和 Regenerator 同时触发时要合并成一次 Emit。
- 伤害修正：各处 `switch (ctx->abilities[x])` 抽成函数，对 4 个特性循环调用；`DamageContext` 加 `passives`。
- 吸收/免疫：`CanAbilityAbsorbMove`、`IsBattlerUngroundedByAbilityItemOrEffect`
- AI：`AiLogicData` 加 `passives`。AI 默认知道被动（被动公开显示）；`AI_DecideKnownAbilityForTurn` 只猜主特性。
- 去重：同一个特性只生效一次，不叠乘。顺序为主特性在前，然后被动 1→3。

## 改特性类机制的规则

| 机制 | 规则 |
|---|---|
| Gastro Acid | 压制全部 4 个 |
| Neutralizing Gas | 压制其他宝可梦的全部特性；持有者自身不受影响；在任意槽都能发动 |
| Skill Swap / Role Play / Entrainment / Simple Beam / Worry Seed / Mummy / Lingering Aroma / Wandering Spirit | 只作用于主特性；判断"已有此特性"时要把被动算进去 |
| Trace / Receiver / Power of Alchemy | 只复制对方主特性；第一阶段禁止它们作为被动 |
| Transform / Imposter | 复制主特性和全部被动 |
| Mold Breaker | 按每个特性的 `breakable` 单独判断；放在被动槽也能生效 |
| Ability Shield | 保护全部 4 个 |

新增 `AbilityInfo.cantBePassive`，标记：Trace、Receiver、Power of Alchemy、Imposter、Illusion、Multitype、RKS System、Zen Mode、Schooling、Shields Down、Stance Change、Disguise、Ice Face、Battle Bond、Power Construct、Zero to Hero、Commander、Tera Shift、Gulp Missile。

## 特性弹窗

`BS_ShowAbilityPopup` 和 `UpdateAbilityPopup` 固定显示主特性。
规则改为：如果 `gLastUsedAbility` 不是主特性、且是该宝可梦的有效被动，就显示 `gLastUsedAbility`。这样不用逐处修改调用点。

## 第一批 10 个被动（覆盖各类触发）

| 被动 | 要验证的点 |
|---|---|
| Intimidate | 入场；槽游标可重入 |
| Drought | 天气 |
| Speed Boost | 回合末 |
| Levitate | 参数传入型判断；可被 Mold Breaker 破 |
| Water Absorb | 吸收 |
| Rough Skin | 命中后；和 Iron Barbs 同时触发 |
| Technician | 伤害修正 |
| Sturdy | 弹窗修正；可被破 |
| Regenerator | 离场 |
| Sheer Force | 全局开关，最容易漏改 |

另加 Mold Breaker，用来测试压制和破特性规则。

## 分阶段

- **S0**：数据字段、测试宏、`cantBePassive`
- **S1**：`BattlerHasAbility*` 系列、压制/破特性、弹窗
- **S2**：5 类事件循环
- **S3**：10 个被动 + 测试
- **S4**：改特性招式的规则
- **S5**：批量迁移剩余的约 500 处判断和 AI 的约 356 处

## Elite Redux（未核实）

印象中 ER 的做法：物种数据里有 `innates[3]`；用 `BattlerHasAbility` 类的宏替换 `== ABILITY_X`；弹窗显示被动名；Skill Swap 只换主特性；Gastro Acid 和 Neutralizing Gas 压制全部。实施前需要实际查看它的代码确认。
