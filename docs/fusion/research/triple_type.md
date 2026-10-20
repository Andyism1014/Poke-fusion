# 阶段 05 调研：永久三属性

> 基线 `f4dc700a`。行号是调研时的位置，实施前需要重新核对。

## 现状

- `BattlePokemon.types[3]`（include/pokemon.h:358），位于 `pp` 之前，Transform 会一起复制。
- `types[2]` 现在只用于 Forest's Curse / Trick-or-Treat 的临时附加属性（`BS_TryThirdType`，battle_script_commands.c:11677）。
- 克制计算 `CalcTypeEffectivenessMultiplierInternal`（battle_util.c:8260）**已经遍历 3 个槽位并去重**。
- STAB 用 `IS_BATTLER_OF_TYPE`，会算上 `types[2]`；Adaptability 和 Tera STAB 也都能正确生效。
- 下面这些地方会把属性重置为"物种属性 + MYSTERY"，都要改成新函数 `InitBattlerTypes`：开场（battle_main.c:3137）、换入（bsc:3240）、形态变化（battle_util.c:9593）、`PokemonToBattleMon`（pokemon.c:3400）、`RESTORE_BATTLER_TYPE` 宏、Mimicry 还原。

**方案**：`BattlePokemon` 在 `types[3]` 后、`pp` 前加 `enum Type fusionType`，由 `PokemonToBattleMon` 填写。`InitBattlerTypes` 设 `types[2] = fusionType`。

## 改属性机制的规则

| 机制 | 规则 |
|---|---|
| Soak / Magic Powder | 清掉融合属性；失败条件补上 `types[2] == MYSTERY` |
| Conversion / Protean / Libero | 变为单属性（现有代码已正确） |
| Reflect Type | 复制目标的 3 个属性 |
| Burn Up / Double Shock | 也会去掉融合属性 Fire/Electric（现有代码已正确） |
| Roost | **需要修**：`GetBattlerTypes` 只处理槽位 0/1，要补上 `types[2] == FLYING` |
| Forest's Curse / Trick-or-Treat | 替换融合属性，换下后恢复（可以做成 config，改为"已有融合属性时失败"） |
| Tera | 覆盖全部属性；Stellar 保留融合属性 |
| Transform / Imposter | 复制 3 个属性 |
| 形态变化 / Mega | 保留融合属性 |
| Color Change | 被融合属性的招式打中不触发（现有代码已正确） |

## UI

- 总结画面 `SetMonTypeIcons`（pokemon_summary_screen.c:4337）：图标宽 32px，放在 x = 120/160/200，Tera 图标占 200。从 120 到屏幕右边只放得下 3 个图标。
  - 方案 A：图标放在 120/152/184，Tera 挪到别处。
  - 方案 B：融合属性占 200，Tera 只在没有融合属性时显示。
- 战斗内属性图标 `type_icons.c:263-277` 只画 2 个，需要扩展到 3 个。
- 招式克制提示已自动正确；图鉴保持物种数据不变。

## AI 需要修的地方

- `SaveBattlerData` / `RestoreBattlerData`（battle_ai_util.c:267, 422）只存槽位 0/1。
- Illusion 模拟（:380）。
- `GetTypeMatchupAgainstTypes` / `GetBattlerTypeMatchup`（battle_ai_switch.c:1991-2022）只看 2 个属性，影响换人决策。
- `RecordMovesBasedOnStab`（battle_ai_main.c:721）用的是物种属性。

## 战斗外

应该改为 `MonHasType(mon, type)` 的地方：

- 进化条件 `IF_TYPE_IN_PARTY`（pokemon.c:4245）
- `IsGrassTypeInParty`
- 天空战斗资格
- 跟随宝可梦的表情

Battle Frontier 的各项设施和游戏内交换可选。野生遭遇、联机、华丽大赛不改。

## 测试用例（`test/fusion/` 或 `test/battle/fusion_type.c`）

1. 融合 Ghost 的 Wobbuffet 免疫 Scratch。
2. 融合 Bug 的 Charizard 受 Rock 招式 8 倍伤害；融合属性和本身属性重复时不叠算。
3. 融合属性招式有 1.5 倍 STAB；有 Adaptability 时为 2 倍。
4. Forest's Curse 替换融合属性，换下后恢复。
5. Soak 清掉融合属性；纯水系 + 融合属性被 Soak 时不失败。
6. Roost 去掉融合 Flying；Burn Up 去掉融合 Fire。
7. Tera 覆盖全部属性；Stellar 保留融合属性。
8. Transform 和 Reflect Type 复制第三属性。
9. 形态变化和 Mimicry 还原后，融合属性仍在。
10. Protean 之后融合属性消失。
11. AI 不对融合 Ghost 使用 Normal 招式；换人时考虑第三属性。
12. Illusion 状态下显示和 AI 推断一致。

可参考：test/battle/move_effect/soak.c、reflect_type.c、roost.c、gimmick/terastal.c、ability/adaptability.c、ai/ai_assume_stab.c。
