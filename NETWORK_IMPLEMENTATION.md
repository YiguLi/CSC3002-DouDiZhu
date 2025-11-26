# 斗地主局域网联机功能实现说明

## 概述
为斗地主游戏添加了完整的局域网联机功能，支持2-3人联机游戏。

## 核心设计原理

### 1. 玩家类型系统
重构了玩家分类方式，不再使用ID判断类型，而是使用 `PlayerType` 枚举：

```cpp
enum PlayerType {
    LocalPlayer,    // 本地玩家（人类）
    NetworkPlayer,  // 网络玩家（其他联机玩家）
    AIPlayer        // AI玩家
};
```

### 2. 联机模式原理
- **房主模式**：创建TCP服务器，等待其他玩家连接
- **加入模式**：作为TCP客户端连接到房主
- **玩家ID分配**：房主ID=0，第一个加入的玩家ID=1，第二个加入/AI的ID=2

### 3. 游戏同步机制
所有玩家各自在本地运行一局完整的游戏，通过网络消息同步操作：

#### 本地玩家操作流程：
1. 本地玩家执行操作（叫地主/出牌/过牌）
2. 操作在本地游戏中生效
3. 通过网络广播操作给其他玩家
4. 其他玩家接收到消息，在各自的本地游戏中应用该操作

#### 网络玩家操作流程：
1. 等待网络消息
2. 接收到网络消息后，在本地游戏中应用该玩家的操作
3. 显示操作结果

#### AI玩家操作流程（仅在二人联机模式）：
1. **房主端**：AI在房主的游戏中自动执行操作
2. 房主将AI操作广播给其他玩家
3. **其他玩家端**：接收AI操作消息，在本地游戏中应用
4. 对于加入房间的玩家，看到的是两个网络玩家（房主+AI）

## 主要实现文件

### 1. `networkmanager.h/cpp` - 网络管理器
负责所有网络通信：
- 创建/加入房间
- TCP连接管理
- 消息序列化/反序列化
- 操作广播和接收

支持的消息类型：
- `MSG_CREATE_ROOM` - 创建房间
- `MSG_JOIN_ROOM` - 加入房间
- `MSG_PLAYER_READY` - 玩家准备
- `MSG_GAME_START` - 游戏开始
- `MSG_CALL_LANDLORD` - 叫地主
- `MSG_DISCARD_CARDS` - 出牌
- `MSG_PASS` - 过牌
- `MSG_AI_ACTION` - AI操作（房主广播）

### 2. `lobbyroom.h/cpp` - 联机房间UI
提供联机功能的用户界面：
- 创建房间界面
- 加入房间界面（输入IP和端口）
- 房间等待界面（显示玩家列表、准备状态）
- 开始游戏按钮（仅房主可用）

### 3. `player.h/cpp` - 玩家类（重构）
- 添加 `PlayerType` 成员和相关方法
- 修改所有基于ID的类型判断为基于 `PlayerType`
- 区分本地玩家、网络玩家、AI玩家的行为

### 4. `game.h/cpp` - 游戏逻辑（扩展）
添加网络模式支持：
- `SetNetworkMode()` - 设置为网络模式
- `SetupNetworkGame()` - 初始化网络游戏（设置各玩家类型）
- `OnNetworkCallLandlord()` - 处理网络叫地主消息
- `OnNetworkDiscardCards()` - 处理网络出牌消息
- `OnNetworkPass()` - 处理网络过牌消息

修改游戏流程以支持联机：
- 本地玩家操作后自动广播
- AI操作由房主广播（二人联机模式）
- 网络玩家等待网络消息

## 使用流程

### 创建房间（房主）
1. 启动游戏，选择"局域网联机"
2. 点击"创建房间"
3. 系统显示房间信息（IP和端口）
4. 等待其他玩家加入（2-3人）
5. 所有玩家点击"准备"
6. 房主点击"开始游戏"

### 加入房间
1. 启动游戏，选择"局域网联机"
2. 输入房主的IP地址和端口（默认12345）
3. 点击"加入"
4. 成功加入后，点击"准备"
5. 等待房主开始游戏

### 游戏模式
- **三人联机**：三个人类玩家联机，无AI
- **二人联机**：两个人类玩家+一个AI（由房主控制并同步）

## 技术特点

1. **状态同步而非数据同步**：每个客户端运行完整游戏逻辑，只同步玩家操作
2. **TCP通信**：使用Qt的 QTcpServer/QTcpSocket 确保可靠传输
3. **JSON消息格式**：使用QJsonDocument序列化消息，易于调试和扩展
4. **房主权威**：房主负责AI逻辑和游戏开始控制
5. **类型安全**：使用枚举而非魔法数字判断玩家类型

## 集成建议

### 在现有UI中集成联机功能：

1. **在主菜单添加"联机模式"按钮**
```cpp
// 在 gamestartscene.cpp 中
QPushButton* btnOnline = new QPushButton("联机模式");
connect(btnOnline, &QPushButton::clicked, this, &GameStartScene::OnOnlineClicked);

void GameStartScene::OnOnlineClicked() {
    LobbyRoom* lobby = new LobbyRoom();
    NetworkManager* netMgr = new NetworkManager();
    lobby->SetNetworkManager(netMgr);
    
    connect(lobby, &LobbyRoom::gameStarting, this, [=]() {
        // 创建网络游戏
        Game* game = new Game();
        game->SetNetworkMode(true);
        game->SetNetworkManager(netMgr);
        
        // 设置游戏
        bool useAI = (netMgr->GetPlayerCount() == 2);
        game->SetupNetworkGame(netMgr->GetLocalPlayerId(), useAI);
        
        // 连接网络事件到游戏
        connect(netMgr, &NetworkManager::callLandlordReceived, 
                game, &Game::OnNetworkCallLandlord);
        connect(netMgr, &NetworkManager::discardCardsReceived, 
                game, &Game::OnNetworkDiscardCards);
        connect(netMgr, &NetworkManager::passReceived, 
                game, &Game::OnNetworkPass);
        
        // 启动游戏界面
        game->GameStart();
        // ... 显示游戏界面
    });
    
    lobby->show();
}
```

2. **处理AI操作接收**（仅客户端）
```cpp
// 在游戏界面中
connect(netMgr, &NetworkManager::aiActionReceived, 
        this, [=](int aiId, MessageType type, const QJsonObject& data) {
    if (type == MSG_DISCARD_CARDS) {
        QJsonArray cards = data["cards"].toArray();
        std::vector<int> cardList;
        for (const QJsonValue& v : cards) {
            cardList.push_back(v.toInt());
        }
        game->OnNetworkDiscardCards(aiId, cardList);
    } else if (type == MSG_PASS) {
        game->OnNetworkPass(aiId);
    } else if (type == MSG_CALL_LANDLORD) {
        int score = data["score"].toInt();
        game->OnNetworkCallLandlord(aiId, score);
    }
});
```

## 注意事项

1. **防火墙设置**：确保防火墙允许程序监听和连接端口12345
2. **局域网IP**：玩家需要知道房主的局域网IP地址（如192.168.1.100）
3. **同步时序**：所有玩家必须按相同顺序接收和处理消息
4. **断线处理**：当前实现会在玩家断线时通知其他玩家，但游戏会中断
5. **发牌同步**：需要确保所有玩家使用相同的随机种子，或由房主分发牌

## 未来改进方向

1. **断线重连**：允许玩家临时断线后重新加入
2. **观战模式**：允许额外玩家作为观众
3. **聊天功能**：添加游戏内文字聊天
4. **排名系统**：记录联机战绩
5. **UDP优化**：对非关键消息使用UDP减少延迟
6. **加密通信**：防止作弊和窃听
7. **房间列表**：局域网自动发现房间

## 编译说明

确保CMakeLists.txt已更新，包含Qt Network模块：
```cmake
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)
target_link_libraries(testing PRIVATE Qt${QT_VERSION_MAJOR}::Network)
```

## 测试方法

### 本地测试（单机模拟）
1. 启动两个游戏实例
2. 第一个实例创建房间
3. 第二个实例使用127.0.0.1加入
4. 开始游戏测试

### 局域网测试
1. 两台电脑连接到同一局域网
2. 查看房主电脑的IP地址（ipconfig/ifconfig）
3. 房主创建房间
4. 其他玩家使用房主IP加入
5. 测试所有游戏流程

---

实现完成！所有核心功能已就绪，可以开始联机对战了。
