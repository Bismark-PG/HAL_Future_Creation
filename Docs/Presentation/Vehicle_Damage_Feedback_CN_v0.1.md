# 通用车辆受击：红色剪影、白色轮廓与局部RGB偏移

2026-11-15，接续main/5044110，UE5.8.3-58210709。用户确认NFS Unbound参考方向及实际扣血、免疫分离、连续命中限幅、全局共用配置和电击叠加方案，授权施工。程序、原生资产、Editor／Game构建、95项回归及80ms双端验证已完成；画风和正式相机体验待团队验收。本轮证据见[可同步验证摘要](Vehicle_Damage_Feedback_Verification_20261115.json)，不以旧铁链／技能结果代替本次测试。

## 1. 触发与生命周期

同日后续构建修复：普通BuildAndLaunch触发新受击配置与警车CPP的匿名辅助函数重名，现已增加明确命名，并修正HUD模板语法警告；字段、DA及效果公式未变。关闭自适应拆分的Editor／Game Unity构建及当前11项专项均通过，见[构建修复记录](../Configuration/Build_Editor_Tool_CN.md)。本页第6节的95项／双端仍为首版功能验证历史，不是这次重跑的数据。

- 唯一正式入口是服务器CombatResolver在GE生效后取得的ActualHPRemoved>0。球、弹簧拳、伤害型车辆接触／技能共用这一结果入口，无需各自填特效引用。
- 不观察HP复制差值推断攻击，不从客户端碰撞宣布受击。重复Action目标由原Combat去重；短表现使用目标组件的递增序号、不可靠Multicast和服务器时间，旧／重复序号丢弃。时间戳用于过期判断，有效事件在各端收到后从峰值起播，避免网络延迟吃掉极短的白边／RGB；这是权威确认后的本地表现，不是受击预测。晚加入不重播过去的短闪。
- 冲击免疫仍扣血则播放；伤害免疫、0伤害、纯击退、普通无伤车撞／墙撞、纯眩晕／逮捕不触发红色受伤反馈。伤害／冲击规则、抗性、HP和技能CD均未由表现修改。
- 每目标独立包络。连续命中刷新红色，白边／RGB受Minimum Accent Interval限制；连续窗口封顶后短暂休息，不限制实际伤害次数。
- 到期归还遮罩；换Avatar／解绑、隐藏、销毁和世界退出清理。未延后淘汰或销毁来保留演出，立即退场的致命受击可能没有可见车身短闪。

## 2. 全局配置与已有车辆

原生资源生成器创建：

| 资产 | 用途 |
| --- | --- |
| `/Game/Data/StatusFX/DA_DamageFeedback_Default` | HALDamageFeedbackConfig，全局唯一配方入口 |
| `/Game/Assets/FX/Materials/M_FX_VehicleDamage_PP` | Post Process材质，组合车辆闪红、白色外轮廓与局部RGB |

**Edit → Project Settings → Game → Vehicle Status FX → Shared Configuration → Damage Feedback Config** 指向上述DA；DefaultGame.ini已纳入配置。**Rendering → Postprocessing → Custom Depth-Stencil Pass** 需要Enabled with Stencil，DefaultEngine.ini使用r.CustomDepth=3。更改渲染项目设置后完整重开Editor。

正式车辆Pawn自动组装DamageFeedback组件，新技能木桩继承同一组件。沿用VehiclePresentation的可见Body和显式注册部件，自动匹配Static／Skeletal模型及姿态，不要求每辆车重接BP或修改车身母材质。旧Passive保留基础回归用途，没有自动添加本轮新接收组件。

修改DA后重开Play再验证。每个本地渲染World只创建一个局部效果通道，各目标有独立时间、遮罩ID和权重；全局共用配方不会让所有车辆一起闪。Dedicated Server／NullRHI不创建此渲染通道。

## 3. DA调参

| Style参数 | 默认值 | 用途 |
| --- | --- | --- |
| Enabled | true | 整套受伤反馈开关 |
| Body Color／Opacity | 红色／0.85 | 闪红颜色与覆盖；保留原画面的大块明暗 |
| Body Seconds／Hold Seconds | 0.2s／0.025s | 红色总时长／峰值保持 |
| Edge Color／White Opacity | 暖白／0.9 | 外轮廓颜色／强度 |
| White Seconds | 0.065s | 白边持续时间 |
| Outline Pixels／Softness | 2／0.5 | 当前渲染视图像素中的粗细／边缘过渡 |
| RGB Opacity／Seconds | 0.6／0.09s | 色散强度／时间 |
| Separation Pixels／Direction | 3／(1,0.25) | 屏幕偏移距离／方向，方向会归一化 |
| Minimum Accent Interval | 0.14s | 白边与RGB再次起播的最短间隔，需≥White／RGB时长 |
| Maximum Continuous Seconds | 0.55s | 连续受击的单段可见上限，需≥三个表现时长 |
| Rest Seconds | 0.15s | 连续窗口封顶后的视觉休息，不影响伤害 |
| Maximum Event Age | 0.5s | 网络／资源准备事件的最大可接受年龄，超时丢弃，不回放旧闪光 |
| First Stencil／Maximum Targets | 240／8 | 本效果遮罩预留范围与同时目标预算，最大8 |
| Depth Tolerance | 1cm | 真实场景深度与车辆CustomDepth的可见性比较容差 |

先调Body Color／Opacity和Body Seconds，再调白边，最后调RGB。RGB和White Opacity可分别置0作层间对比；Body Hold必须小于Body Seconds。预留Stencil范围应与其他需要Custom Stencil的系统协调，不能将同一ID同时用于无关效果。

## 4. 渲染与未来制作约束

三层组合位于After Tonemapping的车辆遮罩局部后处理，不修改基础材质或Overlay。电击／状态系统继续独占原Overlay，其运行时钟与粒子不被取消；短红闪在最终画面临时覆盖车身色，结束后自然显示当时的电击／免疫／原车身。不会占用FX-4的两个状态显示通道。

遮罩绑定只使用当前可见的表现Mesh，不启用隐藏物理根、火花、球、场地和UI。外轮廓／偏移同时比较场景深度，避免透墙；每次租借保存原CustomDepth／Stencil／WriteMask及实际分配ID，到期按所有权检查恢复，途中修改全局ID也不会遗留旧租约。已有CustomDepth使用者不被直接改写，分配时会避开本地World中已占用的ID；项目默认预留240～247。团队不需要逐车手动勾选Render CustomDepth Pass，若其他系统已经占用了该车的CustomDepth，本轮会保留原所有者，可能缺失该部件受击遮罩，需先协调所有权。

半透明玻璃只有在其材质允许Custom Depth Writes时才会完整参与遮罩，本轮不批量改玻璃资产。屏幕UI后于场景合成；世界空间UI或透明粒子恰好覆盖车身像素的组合仍需真人实看。

替换本效果材质时保持Post Process域及参数合同：BodyColor、EdgeColor、EdgeSettings(width,softness,depthTolerance,0)、SplitSettings(dirX,dirY,pixels,0)、Target0～Target7(stencilId,bodyWeight,whiteWeight,rgbWeight)，均为Vector。Target参数必须使用RGBA，不能丢弃A；未激活参数为全0，输出应为未改动的输入场景色。材质只消费只读表现数据，不读写HP、Stun或物理。

全局配方由World初始化异步准备，命中回调不同步加载资产。准备期间最多缓存每目标一条短事件，过期不补播。共享配方及其硬引用材质由现有Cook根收集器验证／收集；Cook／包实际成功仍需单独证据。

## 5. 测试与排查

1. 完整重开Editor，用允许扣血的测试流程，让球／弹簧拳／伤害型车技分别命中玩家和新技能木桩；普通车撞、纯击退、伤害免疫不应误闪红。练习流程禁伤时按实际0伤害处理。
2. 对比单次、连续与同时命中两车，观察红色／白边／RGB分别结束，其他车和地面保持原样；电击／持续状态同时存在时原状态不重计时。
3. 双端互换攻击和受击，检查短闪、遮挡、换车／返房／销毁后的清理。真实晚加入不要求补播旧瞬间。
4. `hal.DamageFeedbackLog 1`记录服务器结果脉冲及本地接收；`hal.DamageFeedbackStatus`检查配方准备、目标／Mesh数量及Pass启用状态。
5. `hal.DamageFeedbackPreview local`或`hal.DamageFeedbackPreview all`为本地纯画面预览，不扣血、不发送RPC；正式伤害链路仍应单独测试。Shipping不提供预览命令。

## 6. 本轮工程验证

- 最终Editor **BuildEditor_R14**、Game **BuildGame_R6**成功；新材质／DA由原生Commandlet创建，未直接修改任何BP或地图。Editor工具模块关闭Unity，避免旧生成工具的匿名辅助函数在合并编译时发生重名。
- **Final_R8共95/95通过**：93项无警告、2项带既有警告，失败／未执行／进行中均0。包含实际扣血／去重／伤害免疫／冲击免疫、独立脉冲／限幅、共享配方／Cook根、FX-4状态、木桩、通缉／Stun、Car03～05、油膜、冲击、基础HP及换车等受影响回归。两项警告分别为换车负例预期诊断（2条）和D3D12累计虚拟资源预算提示（1条），不是零警告通过。
- 实际D3D12离屏图像使用原Car02骨骼模型及原材质，闪红组合改变37070像素；白边独立2658像素、RGB独立2314像素；未受击车辆及邻近地面变化0。整图遮挡与到期后的当前干净基线对照均通过，未排除边框或放宽颜色差阈值。另检查Static目标、多目标独立ID、其他CustomDepth所有者／预留ID碰撞、实际电击Overlay引用保留、过期事件丢弃及租约归还。
- 图像夹具最初缺少持久ViewState和正确曝光；后续定位到UE色调映射的8位后缓冲量化抖动，它独立于Film Grain开关。最终仅在测试作用域内将`r.BackbufferQuantizationDitheringOverride`临时设为32并还原，关闭夹具AA／屏幕空间历史效果；正式游戏渲染设置没有这些测试修改。遮挡比较同一后处理链的零／有效事件权重；清理在移除遮挡墙后的同一几何状态取基线。试验用稳定车身材质已撤回，最终截图沿用原材质。
- 扩展状态回归暴露旧逮捕时间查询在GC阶段World为空，以及旧Static采样夹具使用未初始化RenderData。分别补空World防守、改用已初始化引擎Cube并恢复临时CPU Access标志；状态专项39/39后纳入最终95项。未改变逮捕、眩晕或伤害规则。
- **Network_R1实际D3D12 Listen Host／Client各1/1通过**，0测试警告／失败、退出0，Host出站80ms。两端均记录`received=1 rendered=1 cleanup=1 accent=1`，确认权威扣血脉冲、本地Pass及短白边／RGB权重、到期清理；原木桩HP、通缉、铁链／解除和HUD回归继续通过。双端短闪依据实际RHI组件／材质权重观测，逐像素图像另由上述离屏用例验证，不宣称双端逐帧截图验收。
- 开工1336份既有Content／Config／uproject及Passive源码中，1334份哈希一致，只有DefaultEngine.ini（Stencil）和DefaultGame.ini（共享DA引用）按本任务变更；新增2份uasset。用户原有GameMode BP、车型Definition／技能／表现DA、原材质／Mesh／地图及旧Passive均保留。

原始报告／BMP位于本机`Saved/DamageFeedback/20261115`，不纳入Git；本页与验证摘要已加入项目白名单，仍需正常提交／Push后才能跨设备同步。

## 7. 团队下一步

完整重开本轮Editor，直接用原技能／球／机关测试，无需为每辆车新增BP接线。推荐先用`hal.DamageFeedbackPreview all`看静态轮廓，再测试实际伤害与两端观察。DA调参后重新Play，先减弱或增强Body Opacity，再调整White Seconds／Outline Pixels，最后按实际相机调整Separation Pixels。若白边／色散感受过强，可各自把Opacity设为0对照。

仍待团队：正式相机和地图下的卡通画风、各车型半透明玻璃／复杂附属部件、世界空间UI／透明粒子重叠、四机、真实晚加入、丢包与帧率体验，以及Cook／打包运行。本轮只验证Cook引用根，不等于完成Cook或发布包；没有延后淘汰演出，也未进行专项GPU耗时测量。近乎同时大量受击受8目标预算限制，表现降级不影响实际伤害。
