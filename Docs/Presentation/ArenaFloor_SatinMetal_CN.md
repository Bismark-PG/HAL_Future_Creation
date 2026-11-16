# MVPB_Match 波纹缎面金属地面

## 资产与接线

| 项目 | 路径或值 |
|---|---|
| 母材质 | /Game/Assets/Materials/M_ArenaFloor_Metal |
| 材质实例 | /Game/Assets/Materials/MI_ArenaFloor_Metal |
| 当前法线 | /Game/Assets/Textures/T_ArenaFloor_RandomRelief_N |
| 竞技场中央可见网格 | base_circle_inflatable_007_1374 → Static Mesh Component → Materials → Element 0 |

关卡中名为 Floor 的组件关闭了可见性，中心位置的碰撞 Trace 命中该组件。Path 使用半透明竞技场护盾材质。预览时仅在内存里替换中央可见网格的 Element 0，并创建测试面光。

材质使用 Opaque / Default Lit，Metallic 为 1，Roughness 为 0.54，Metal Color 为线性 RGB 0.16 / 0.21 / 0.29。Arena Pattern Amount 默认 0.50，从原场地颜色图集的 UV0 采样并与底色混合。设为 0 可得到纯色金属。

法线的 X/Y 表示地面沿世界 +X/+Y 的坡度，主纹理按世界 XY 坐标采样。Wave Strength 默认 0.58，高光起伏过强时可从 0.45 试起。

## 随机凹凸法线

母材质的三处 Wave Normal 采样引用 T_ArenaFloor_RandomRelief_N，实例从母材质继承。贴图由固定种子分布的多尺度正负起伏生成，位置、尺寸、长宽比和朝向独立变化。贴图尺寸为 1024 × 1024，UE Normalmap 压缩、sRGB 关闭、XY 循环。原 T_ArenaFloor_Wave_N 可用于对比。

使用同一保存场地、俯视镜头、灯光和曝光，对比了旧方向波纹与新随机凹凸。采样参数为主纹理 850 cm、放大 2300 cm，细节采样 200 cm。新版呈现不规则斑状高光，放大后仍有细小起伏，并随原覆盖渐变淡出。源图法线方向偏向指标从 0.768 降至 0.031，数值越接近 0 表示坡向越均匀。UE 5.8.3 保存命令退出 0，SM6 无材质编译错误，实例重载验证三处采样及 1024 × 1024 贴图。对比图位于 Saved/Inspections/ArenaMetalFloor/RandomRelief，正式比赛镜头待确认。

## 波纹区域过渡和近看细节

当前保存图的两个 Noise 原先使用 6 层、−1～1，正阈值把覆盖范围集中在较高的噪声值附近。现在覆盖和疏密各使用 1 层 Simplex、0～1。覆盖路径保留原来的世界坐标映射，遮罩由 SmoothStep 连续控制。疏密仍由原两路 Normal RGB 的 Lerp 控制。

| 实例参数 | 默认值 | 作用 |
|---|---|---|
| Wave Coverage Start | 0.30 | 低于此值时，波纹强度为零。提高可增加完全平滑的区域。 |
| Wave Coverage Feather | 0.42 | 遮罩的渐变宽度。默认完整波纹强度的阈值是 0.72。 |
| Wave Tile Size Cm | 850 | 控制原有两路主波纹的平铺范围，保留疏密变化。 |
| Wave Detail Tile Size Cm | 200 | 控制小波纹的世界平铺尺寸，独立于主波纹。 |
| Wave Detail Strength | 0.28 | 控制小波纹相对主波纹的强度。 |

细节采样复用同一 Wave Normal 贴图，按世界 XY / Detail Tile Size 采样，并附加固定的 UV 偏移。Custom 先叠加主波纹与细节的 XY 斜率，再一起乘以原 Wave Strength 和覆盖遮罩，最后归一化到世界法线。这样细节也随边界淡出。

在 UE 5.8.3 以同一俯视角、曝光和灯光捕获修改前后画面，并对比主平铺尺寸 2300 cm 时启用及关闭细节层的结果。同一地面区域有 1031 / 9775 个采样位置的亮度变化至少 6 / 255，横向每 3 像素的平均亮度差从 1.04 升至 1.89。保存后重新加载验证参数及节点连接，PCD3D_SM6 无材质编译错误。保存前的中间接线曾触发一次缺输入警告，最终重载没有该警告。修改前预览写图后异常退出，修改后预览写出五张截图，再关闭专用会话。地图、实例与法线贴图开工及结束的 SHA-256 分别一致。预览留档于 Saved/Inspections/ArenaMetalFloor，尚待正式比赛相机下验收。

## −75° 俯视角高光

矩形面光放在需要出现高光的位置上方，水平面上位于镜头相反的一侧，朝向目标地面。局部波纹改变法线，在法线接近光源方向和观察方向的夹角平分线时，反射会进入镜头。

| UE 属性 | 实测起点 |
|---|---|
| Rect Light Transform | Location 2000, −670, 1653.5 cm；Rotation Pitch −71.5° / Yaw 180° / Roll 0° |
| Source Width / Height | 1280 / 300 cm |
| Intensity / Attenuation Radius | 6000 lm / 4500 cm |
| Rect Light Mobility / Lighting Channels | Movable；仅 Channel 1 |
| 中央可见网格 Mobility / Lighting Channels | Movable；Channel 0 与 Channel 1 |
| Indirect Lighting Intensity / Diffuse Scale / Specular Scale | 0 / 0 / 1 |
| Cast Shadows / Volumetric Scattering Intensity | Off / 0 |

测试位置对应地面坐标 1380, −670, −196.5 cm。地面 Channel 0 继续接收现有光源。通道控制动态直射；场景的 Lumen 间接光与反射仍需在比赛镜头中检查。地面上局部过亮时，可优先降低 Rect Light 强度，或者沿地面平移灯具变更高光落点。整张画面随镜头变亮时，固定曝光对比可帮助判断变化来自曝光还是灯光。

## 2026-11-15 验证

UE 5.8.3-58210709，DX12。通过引擎接口创建三份资产，关闭后重载验证贴图压缩、sRGB、实例 Parent、四项标量与四条输出。渲染稳定后报告贴图 512 × 512。临时场景捕获涵盖波纹法线强度 0 与 0.58、Rect Light 开关、约 −75° 与 −84°。固定曝光、关闭 Bloom 的截图抽样中，加灯后中央地面区域亮度平均增加 100.43 / 255，峰值 216 / 255；机关抽样区域平均变化 −3.42 / 255，前后均无亮度 ≥248 / 255 的像素。对比固定相机下的平法线，观察到波纹形成的高光变化。

素材生成命令正常退出。最终截图序列全部写出后，预览 Editor 在 Python 回调退出阶段异常退出。正式关卡无保存，现有地图 SHA-256 与开工一致。本轮证据位于 Saved/Inspections/ArenaMetalFloor；实机比赛摄像机、自动曝光、整场周围物体与打包仍待复核。
