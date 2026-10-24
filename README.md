# CharacterTrailFix（YoumuTrailFix）

东方非想天则（th123）的**残影（Trail）修复与扩展插件**：

- 修复妖梦 6D 残影**仅在"红魔馆大堂"可见**的原生渲染 BUG，使残影在所有地图正常显示；
- 将引擎内置、但几乎不可见的残影系统开放为可配置功能：颜色、间隔、混合模式、驻留式残影、彩虹渐变等均可通过 ini 定制。

下文统一以 `YoumuTrailFix` 指代本插件。

适用游戏版本：**东方非想天则 1.10a**（32 位）。

---

## 背景：原生残影系统与渲染 BUG

### 引擎内置的"冲刺残影"系统

非想天则的 `Player` 对象内部（偏移 `0x714`）自带一套残影系统。本工程将其结构化为 `TrailImage`（见 [src/main.cpp](src/main.cpp)）：

```cpp
struct TrailImage {
    struct SpriteTrail : public SpriteEx {
        byte blendMode;        // 该残影帧的混合模式
        char align[3];
    };
    SokuLib::List<SpriteTrail> sprites; // 残影帧容器
    byte step;                 // 渲染间隔
    byte personal_stepIndex;   // 本工程挪用：步进索引改写
    short maxSize;             // 容器容量
    DWORD color;               // 残影颜色
    bool personal_rainbowOn;   // 本工程挪用：彩虹模式开关
    byte personal_hueStep;     // 本工程挪用：色相步进
    unsigned short personal_hue; // 本工程挪用：当前色相
};
```

工作方式：角色每帧渲染前，当前帧的 Sprite 快照会被复制进 `sprites` 容器；当角色的残影倒计时字段非零时，游戏会按 `step` 间隔、以递减的透明度，在角色背后逐一渲染容器内的残影帧。

妖梦的 6D 正是通过 `Player::SetTrail(timer, step, color)`（`0x56d649`）设置了这套残影参数——也就是说，**6D 残影不是渲染故障，而是黄昏无意间保留的原生功能**。

### 为什么"有且仅有红魔馆大堂能看见"？

四个事实叠加导致了这个隐蔽的渲染 BUG：

1. 渲染残影帧时，默认混合模式为**减法混合**；
2. 向容器添加残影帧时，**没有重新设置 Sprite 颜色**；
3. 添加残影帧之前，该 Sprite 会先被用于**渲染角色影子**；
4. 除红魔馆大堂外，各地图的影子是纯黑 `#000000`；而红魔馆大堂"影子"实际是地板反光的**倒影**，渲染后遗留的混合色为白 `#FFFFFF`。

于是：残影帧进入容器前颜色已被影子渲染污染为纯黑，减法混合黑色等效于混合 0。因此残影在绝大多数地图上化为棍木，只在红魔馆大堂因遗留白色而意外可见。本插件通过在快照复制后重设颜色修复了颜色污染，让原生残影在所有地图上按设计意图显示。

由于残影完全由 `Player` 的原生字段驱动，可以直接用 SL 脚本改写残影倒计时以及 `TrailImage` 的 `step`、`color` 等参数，为任意动作添加残影效果，参见 [SL 脚本应用示例](#sl-脚本应用示例)。

---

## 功能特性

| 功能 | 对应配置 | 说明 |
|---|---|---|
| **修复渲染** | `[RenderFix]` | 修复残影帧颜色污染，使残影在所有地图可见；`Color` 为全局颜色乘数（可用来调整整体透明度） |
| **自动插入残影** | `[AutoTrail]` | 接管或删除残影帧的插入逻辑，并可统一改写混合模式（默认混合模式 2 几乎不可见） |
| **妖梦残影自定义** | `[Youmu6DTrail]` | 自定义妖梦 6D 残影的间隔与遮罩颜色，使其更明显 |
| **残影类型** | `[RenderFix] StepType` | 驻留式/跟随式：原版为“开火车”的跟随式，驻留式残影帧原地驻留，而非跟随角色移动（大间隔时观感更佳） |
| **RIV 减速适配** | `[Others] TryFitRivDecerlerate` | 依据游戏帧计数器去重，避免减速/暂停时同一帧被重复插入残影 |
| **全角色调试** | `[Others] Debug` | 对全体角色强制开启残影（两名玩家使用不同掩码色以便区分），并禁用残影清理逻辑 |
| **斯安威斯坦**（实验） | `[Others] Sandevistan` | 色相逐帧步进的彩虹残影，可配合 Debug 使用 |



---

## 安装与使用

### 前置要求

- 东方非想天则 **1.10a**（插件通过 `CheckVersion` 与 SokuLib `targetHash` 比对校验游戏版本，版本不符则不加载）；
- SWRSToys 系 ModLoader：插件以 MODULE DLL 形式导出 `Initialize` / `CheckVersion` / `getPriority` 供加载器调用。

### 部署

将构建产物放入游戏目录：

```
th123/
└─ modules/
   └─ YoumuTrailFix/
      ├─ YoumuTrailFix.dll
      └─ YoumuTrailFix.ini
```

ini 与 dll 同目录随插件一起发布；ini 中缺失的键将回退到代码内置默认值（见 [src/main.hpp](src/main.hpp) 的配置定义），修改 ini 后需重启游戏生效。

### 配置说明（ini）

完整键值参见 [src/YoumuTrailFix.ini](src/YoumuTrailFix.ini)。

#### `[AutoTrail]` — 残影帧插入接管

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `Enabled` | 0/1 | `1` | 是否放行残影帧的自动插入。设为 `0` 时本插件直接**丢弃**所有残影帧（相当于关闭系统残影） |
| `BlendMode` | int | `2` | 插入时强制使用的混合模式（可选 0:none/1:add/2:sub）；`-1` 表示保持游戏传入的原模式不变。原生默认走模式 2，但几乎不可见。建议设置为 1。 |

#### 。`[RenderFix]` — 颜色修复

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `Enabled` | 0/1 | `1` | 是否启用颜色修复（关闭即回到原生"仅红魔馆大堂可见"的行为） |
| `Color` | 色彩整数（AARRGGBB） | `0x40ffffff` | 全局颜色乘数，常用来压低残影整体不透明度（alpha 通道在最高字节） |
| `StepType` | 0/1 | `0` | 残影帧步进方式：`0` 原版行为，残影随角色移动；`1` 残影原地驻留不动（`step` 数较大时更有造型感；`step=1` 时两者无差别） |

#### `[Youmu6DTrail]` — 妖梦 6D 残影定制

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `Enabled` | 0/1 | `1` | 设为 `0` 时妖梦 6D 不再产生残影 |
| `Step` | int | `4` | 残影渲染间隔（原生为 4，数值越大越稀疏） |
| `Color` | 色彩整数 | `0x00ffff00` | 残影对原 Sprite 的**遮罩通道**（通道顺序 A/R/G/B）。注意：alpha 位非零会造成不透明度溢出 |

#### `[Others]` — 其他

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `Debug` | 0/1 | `0` | 调试模式：对**所有角色**常开残影，默认 `step=1` 逐帧生成，并按 `teamId` 分别写入 `0x00ff00ff` / `0x0000ffff` 掩码色以区分两名玩家；叠加 `Sandevistan` 时改为 `step=4`、颜色 `0x3Fffffff` 并开启彩虹配色、容量扩至 30；<br />同时，改写清理判断让残影不被销毁 |
| `TryFitRivDecerlerate` | 0/1 | `1` | 适配 RIV 减速/暂停：按游戏全局帧计数（`0x8985d8`）去重，帧计数未推进时不再插入新残影帧 |
| `Sandevistan` | 0/1 | `0` | 实验性彩虹残影：`RenderFix` 生效时叠加逐帧色相步进；配合 `Debug` 使用可见效 |

---

## 技术说明：相关实现细节

插件在 `Initialize` 中对 `.text` 段临时开启 `PAGE_EXECUTE_WRITECOPY`，随后通过 SokuLib 的 `TamperNearCall` 改写以下 call 目标（见 [src/main.cpp](src/main.cpp)）：

| 地址 | 原函数 | 替换函数 | 作用 |
|---|---|---|---|
| `0x4634ce` | `CSpriteEx_copy(CSpriteEx*, CSpriteEx*)` | `RenderFixer` | 快照复制完成后重设残影帧颜色（颜色修复核心） |
| `0x46a71c` | `PlayerTrail_addNode(TrailImage*, SpriteEx*, byte mode)` | `AutoTrailer` | 控制残影帧插入：开关、改写 `mode`、帧去重、调试参数注入 |
| `0x56d649` | `Player::SetTrail(Player*, int timer, int step, uint color)` | `YoumuTrailer` | 覆盖妖梦 6D 的残影参数（`Step`/`Color`） |
| `0x47a9bf` | `CBattleManager::RenderBattle` | `TotalRenderer` | 每帧记录全局帧计数，供减速去重使用（仅 `TryFitRivDecerlerate=1` 时安装） |

除 call 改写外还有两处指令级修改：

| 地址 | 修改 | 作用 |
|---|---|---|
| `0x46a728` | 直接把 `jg` 改写为 `jmp`（`0xEB`） | `Debug=1` 时短路残影容器清理判断，令残影常驻 |
| `0x463173` | `TrampTamper<5>` 蹦床劫持 `ADD ECX,-1 / CMP EDX,ECX` | `StepType=1` 时对步进索引做 `% step` 取模，实现驻留式残影 |

- **`RenderFixer` 中的 `this` 还原**：`CSpriteEx_copy` 的源指针 `character` 实际嵌在 `Player` 内部（偏移 `-4`），借此回溯宿主 `Player` 并取到其 `TrailImage`。

- **蹦床工具**（[src/utils/tramphook.hpp](src/utils/tramphook.hpp)）：`TrampTamper<N>` 生成 `pushad / mov ecx,esp / call hook / popad / 原始指令 / jmp back` 的 shim，hook 函数以 `CPUState*`（寄存器快照）读写 `edi`、`ecx` 等寄存器；shim 由线程安全的 `TrampolinePool` 分页分配，也可用 `TRAMPPOOL_NO_LOCK` 关闭加锁。

- **配置框架**（[src/config.hpp](src/config.hpp)）：自实现的 C++20 编译期字符串字面量模板配置（`iniProxy["RenderFix"_l]["Color"_l]`），直接映射 Win32 `GetPrivateProfileInt/String`，读取失败时保持代码内置默认值。

- **字段复用**：`TrailImage` 中带 `personal_` 前缀的字段复用了原结构中闲置的保留字节（原 `unknown721` / `unknown728` / `unknown729`），未改动游戏内存布局，也无需额外分配；逐字节布局见下节。

### 字段布局：TrailImage 与 personal 扩展

残影状态全部内嵌于 `Player` 对象。`Player + 0x714` 起为 `TrailImage`（SokuLib 中原名 `unknown714`），本插件将其中游戏未使用的保留字节接管为 `personal_` 扩展字段，**没有移动或改变任何原生字段**，因此旧存档、录像与其他 Mod 对原偏移的读写不受影响。

| Player 偏移 | TrailImage 偏移 | 字段 | 大小 | 来源 | 实际作用 |
|---|---|---|---|---|---|
| `+0x710` | — | `trailTimer` | int | 原生（原 `unknown710`） | 残影倒计时，逐帧递减；非零时渲染残影。`Player::SetTrail` 的第 1 参 |
| `+0x714` | `+0x00` | `sprites` | 12 B | 原生（原 `unknown714`） | `List<SpriteTrail>` 残影帧容器；节点 = `SpriteEx` 快照 + 每帧各自的 `blendMode` |
| `+0x720` | `+0x0C` | `step` | byte | 原生（原 `unknown720`，默认 5） | 残影采样/渲染间隔，`SetTrail` 的第 2 参 |
| `+0x721` | `+0x0D` | `personal_stepIndex` | byte | **本插件复用**（原 `unknown721`） | 驻留式残影（`StepType=1`）的步进计数器：每次插入残影帧时 `-1`；`0x463173` 蹦床把 `personal_stepIndex % step` 写回 `ECX` 参与原生比较，使残影帧原地驻留而非跟随角色位移 |
| `+0x722` | `+0x0E` | `maxSize` | short | 原生（原 `unknown722`，默认 15） | 残影帧数量上限 |
| `+0x724` | `+0x10` | `color` | DWORD | 原生（原 `unknown724`） | 残影对原 Sprite 的遮罩颜色（通道顺序 A/R/G/B），`SetTrail` 的第 3 参 |
| `+0x728` | `+0x14` | `personal_rainbowOn` | bool | **本插件复用**（原 `unknown728`，初始化写 0） | 斯安威斯坦彩虹开关：为 `true` 时，`RenderFixer` 每帧将当前色相 `tint_hue(personal_hue)` 乘入最终残影颜色 |
| `+0x729` | `+0x15` | `personal_hueStep` | byte | **本插件复用**（原 `unknown729` 首字节） | 每帧色相步进量（度）；`Debug`+`Sandevistan` 时写 7 |
| `+0x72A` | `+0x16` | `personal_hue` | ushort | **本插件复用**（原 `unknown729` 后 2 字节） | 当前色相（0–359，自动回绕），逐帧 `+= personal_hueStep` |

> 提示：`Player::SetTrail` 函数体入口为 `0x46a750`；技术表中的 `0x56d649` 是妖梦动作脚本里的调用点（本插件的 `YoumuTrailer` 挂钩处）。脚本直接调用时请使用函数入口 `0x46a750`（thiscall，3 参数）。

---

## SL 脚本应用示例

残影参数就在 `Player` 对象的可读写内存里，因此 shady-loader 的 lua 脚本可以直接驱动它。

以下是一个完整示例，使擦弹状态产生残影：

```lua
-- void __thiscall Player_SetTrail(Player *this, int timer, int step, uint color)
local _setTrailImage = memory.createfunccall(0x46a750, 3, true)  -- thiscall，3 参数

-- 参数传 nil 表示“保持该项当前值不变”
local function setTrailImageEx(player, timer, step, color, rainbowOn)
    timer     = timer     or memory.readint(player.ptr + 0x710)                 -- trailTimer
    step      = step      or string.unpack("=b", memory.readbytes(player.ptr + 0x720, 1)) -- step
    color     = color     or memory.readint(player.ptr + 0x724)                 -- color
    rainbowOn = rainbowOn or memory.readbytes(player.ptr + 0x728, 1) ~= "\0"    -- personal_rainbowOn
    _setTrailImage(player.ptr, timer, step, color)
    memory.writebool(player.ptr + 0x728, rainbowOn)  -- 直接写 personal_rainbowOn
end

local function checkGrazing(player)
    local fd = memory.readint(player.ptr + 0x158)     -- 当前帧数据 FrameData
    if fd ~= 0 and (memory.readint(fd + 0x4C) & 0x400) ~= 0 then
        return true                                   -- 帧标志位含“擦弹”
    end
    if player.grazeTimer > 0 then
        return true                                   -- 或处于擦弹判定窗口
    end
end

-- 挂钩 0x46dfae：player 更新后、取消判定前，每帧执行
memory.hookcall(0x46dfae, memory.createcallback(0, function(state)
    local player = battle.Player.fromPtr(state.ecx)
    if checkGrazing(player) then
        -- 正在擦弹：倒计时 2f、逐帧采样（step=1）、蓝绿色遮罩、彩虹关
        setTrailImageEx(player, 2, 1, 0x05FBF, false)
    else
        -- 未擦弹：倒计时清零即停止残影，其余参数不动
        setTrailImageEx(player, 0, nil, 0x000000, nil)
    end
end), 0)
```

要点解读：

- **`rainbowOn` 走内存直写**。脚本在调用`SetTrail`后用 `memory.writebool(player.ptr + 0x728, …)` 单独写 `personal_rainbowOn`；把 `0x729` / `0x72A` 的 `personal_hueStep` / `personal_hue` 也写入（如 `memory.writebyte(player.ptr + 0x729, 15)`），即可脚本化控制彩虹的变色速度。
- **timer 语义**。每次重新调用 `SetTrail` 相当于续期：示例中每帧把 `timer` 刷成 2，残影便持续存在；一旦停止续期，倒计时耗尽自动消散。

---

## 构建指南

### 依赖

- Windows + **Visual Studio 2022**（MSVC，工具集 v143）
- **CMake ≥ 3.19**
- 目标平台：**Win32（x86）**，C++20 / C17
- git 子模块：`src/SokuLib` → [jyanf/Custom_SokuLib](https://github.com/jyanf/Custom_SokuLib)（`dev` 分支，内含 Direct3D 9 头文件与导入库）

### 步骤

```bat
git clone --recursive <本仓库>
cmake -A "Win32" -S . -B out -G "Visual Studio 17 2022"
cmake --build out --config Release --target YoumuTrailFix
```

也可用 [CMakeSettings.json](CMakeSettings.json) 中的 `x86-Debug` / `x86-Release`（Ninja）配置直接在 VS 中打开文件夹构建。

### 构建选项与自动部署

| 选项 | 默认 | 说明 |
|---|---|---|
| `GAME_FOLDER_TH123` | `th123` | 游戏目录；构建前自动 `taskkill th123.exe`，构建后自动执行 `cmake --install`，把 dll、pdb、ini 安装到 `<游戏目录>/modules/YoumuTrailFix/` |
| `I_INI_FILENAME` | `YoumuTrailFix.ini` | ini 文件名（编译期注入 `INI_FILENAME` 宏） |
| `BUILD_MOD_DAT` | `OFF` | 启用后用 `shady-cli.exe pack` 将 `src/data` 打包为 `YoumuTrailFix.dat` 一并安装 |
| `TRAMPPOOL_NO_LOCK` | `OFF` | 单线程场景下省去蹦床池互斥锁开销 |

其他构建细节：静态链接 MSVC 运行时（`MultiThreaded`）；Release 同样生成 PDB（`/DEBUG /OPT:REF /OPT:ICF`）；版本号按 `主版本.构建日.构建年` 自动生成并写入版本资源（[src/version.rc.in](src/version.rc.in)）。

---

## 致谢与相关链接

- [SokuLib](https://github.com/SokuDev/SokuLib) — 非想天则逆向要素库（PinkSmile / SokuDev），本工程链接其自定义分支 [Custom_SokuLib](https://github.com/jyanf/Custom_SokuLib)
- @enebe-nb 仙师
- 负责写 README 的 Qwen
- 《非想天则BUG日记·第九则——残影与七星》— 残影系统字段语义与渲染 BUG 原理的详细分析

## 许可证

MIT License（见 [LICENSE](LICENSE)）。Copyright (c) 2023 SokuDev；版本资源署名 Fishuwako。
