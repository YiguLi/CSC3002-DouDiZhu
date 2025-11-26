# 联机模式使用说明

## 已完成功能

✅ 主菜单添加"联机模式"按钮
✅ 联机房间创建/加入界面
✅ 网络通信完整实现
✅ 玩家操作同步机制
✅ AI操作广播（二人联机）

## 如何使用

### 1. 启动游戏
运行游戏后，主菜单现在有三个按钮：
- **单机模式** - 原有的单机游戏
- **联机模式** - 新增的局域网对战
- **退出** - 退出游戏

### 2. 创建房间（房主）
1. 点击"联机模式"
2. 点击"创建房间"
3. 记下显示的端口号（默认12345）
4. 告诉其他玩家你的局域网IP地址
   - Windows: 打开CMD，输入 `ipconfig`，找到IPv4地址（如192.168.1.100）
   - 也可以使用 `127.0.0.1` 在同一台电脑测试
5. 等待其他玩家加入（2-3人）
6. 所有人准备后，点击"开始游戏"

### 3. 加入房间
1. 点击"联机模式"
2. 输入房主IP地址（如192.168.1.100）
3. 输入端口号（默认12345）
4. 点击"加入"
5. 成功加入后点击"准备"
6. 等待房主开始游戏

### 4. 游戏模式
- **三人联机**: 三个人类玩家，无AI
- **二人联机**: 两个人类玩家 + 一个AI（房主控制）

## 当前状态

网络通信框架已完全实现，但游戏界面（IngameScene）需要重构才能完全集成：

### 问题
`IngameScene` 类使用内部成员 `Game m_game`，无法接受外部Game实例。

### 临时方案
点击"开始游戏"后会显示一个信息框，确认联机已建立，但不会进入游戏界面。

### 完整集成方案

需要修改 `ingamescene.h`：

```cpp
class InGameScene : public QDialog
{
    Q_OBJECT

public:
    explicit InGameScene(QWidget *parent = nullptr);
    explicit InGameScene(Game* externalGame, QWidget *parent = nullptr); // 新增构造函数
    ~InGameScene();
    
    void setGame(Game* game); // 新增方法

private:
    Ui::InGameScene *ui;
    
    Game* m_gamePtr;              // 改为指针
    bool m_usingExternalGame;     // 标记是否使用外部Game
    
    // ... 其他成员
};
```

修改 `ingamescene.cpp`：

```cpp
InGameScene::InGameScene(QWidget *parent)
    : QDialog(parent), ui(new Ui::InGameScene), 
      m_gamePtr(new Game()), m_usingExternalGame(false)
{
    ui->setupUi(this);
    // ... 现有初始化代码
}

InGameScene::InGameScene(Game* externalGame, QWidget *parent)
    : QDialog(parent), ui(new Ui::InGameScene),
      m_gamePtr(externalGame), m_usingExternalGame(true)
{
    ui->setupUi(this);
    // ... 初始化代码，但不调用 m_gamePtr->GameStart()
    setupUIForCurrentGame(); // 直接根据现有game状态设置UI
}

void InGameScene::setGame(Game* game)
{
    if (m_gamePtr && !m_usingExternalGame) {
        delete m_gamePtr; // 清理旧的内部game
    }
    m_gamePtr = game;
    m_usingExternalGame = true;
    setupUIForCurrentGame();
}

InGameScene::~InGameScene()
{
    delete ui;
    if (!m_usingExternalGame && m_gamePtr) {
        delete m_gamePtr;
    }
}

// 所有使用 m_game 的地方改为 m_gamePtr 或 (*m_gamePtr)
```

然后在 `gamestartscene.cpp` 中：

```cpp
// 创建游戏界面
IngameScene* gameScene = new IngameScene(game);
gameScene->show();

// 关闭房间和主菜单
lobby->close();
this->close();
```

## 测试建议

### 本地测试（单机）
1. 运行两个游戏实例
2. 第一个实例：联机模式 → 创建房间
3. 第二个实例：联机模式 → 加入房间（IP: 127.0.0.1）
4. 测试房间功能和网络连接

### 局域网测试
1. 两台电脑连接同一WiFi/局域网
2. 房主创建房间，查看并记录IP
3. 其他玩家使用房主IP加入
4. 测试完整联机流程

## 网络架构说明

- **协议**: TCP (QTcpServer/QTcpSocket)
- **消息格式**: JSON
- **默认端口**: 12345
- **同步方式**: 操作同步（每个客户端运行完整游戏逻辑）

## 下一步开发

1. **重构IngameScene** - 支持外部Game实例（优先级：高）
2. **完整UI集成** - 联机游戏进入游戏界面
3. **断线重连** - 处理网络中断
4. **游戏同步优化** - 确保所有客户端状态一致
5. **添加聊天功能** - 游戏内交流
6. **房间列表** - 自动发现局域网房间

## 注意事项

⚠️ 确保防火墙允许端口12345
⚠️ 所有玩家必须在同一局域网
⚠️ 当前版本房主离开会导致游戏中断
⚠️ 发牌随机性需要同步机制（待实现）

---

联机功能框架已完全实现，等待游戏界面重构后即可完整使用！
