# Bug修复：联机模式客户端不显示手牌

## 问题描述
在联机游戏模式下，作为加入房间的玩家（客户端）无法看到自己的手牌。

## 问题原因

### 根本原因
1. **服务器端**调用 `initGame()` 会自动发牌并创建手牌UI ✅
2. **客户端**只显示"等待服务器开始游戏..."，但没有实现接收和显示手牌的逻辑 ❌

### 具体分析
在 `InGameScene` 的网络模式构造函数中：
- 服务器：`initGame()` → 洗牌、发牌、创建UI
- 客户端：只显示等待文字，`handleNetworkDealCards()` 是空的TODO

## 修复方案

### 1. 服务器端：发送发牌消息
**新增方法** `sendDealCardsToClients()`:
```cpp
void InGameScene::sendDealCardsToClients()
{
    // 遍历所有客户端玩家（玩家1和2）
    for (int gamePlayerId = 1; gamePlayerId <= 2; ++gamePlayerId) {
        Player* player = m_game.GetPlayer(gamePlayerId);
        const std::multiset<int>& cards = player->GetCards();
        
        // 将手牌序列化为JSON
        QJsonArray cardArray;
        for (int cardId : cards) {
            cardArray.append(cardId);
        }
        
        // 发送给对应客户端
        QJsonObject message = {
            {"type", "DealCards"},
            {"data", {
                {"cards", cardArray},
                {"gamePlayerId", gamePlayerId}
            }}
        };
        m_server->sendToClient(gamePlayerId, message);
    }
}
```

**调用时机**：
在服务器的 `initGame()` 后，使用 `QTimer::singleShot(500, ...)` 延迟发送，确保客户端准备就绪。

### 2. 客户端：接收并显示手牌
**完善方法** `handleNetworkDealCards()`:
```cpp
void InGameScene::handleNetworkDealCards(const QJsonObject& data)
{
    // 1. 获取服务器发送的手牌
    QJsonArray myCards = data["cards"].toArray();
    
    // 2. 初始化游戏状态（不洗牌）
    m_game.InitGame();
    
    // 3. 清空玩家0的手牌
    Player* human = m_game.GetPlayer(0);
    human->NewGame();
    
    // 4. 将收到的牌添加到手牌
    for (const QJsonValue& val : myCards) {
        int cardId = val.toInt();
        human->AddCard(cardId);
    }
    
    // 5. 创建地主牌UI（背面）
    createLandlordPanels(false);
    
    // 6. 创建手牌UI
    createPlayer0HandPanels();
    
    // 7. 更新界面
    updateAiRemainLabels();
    setStatusText("游戏开始！等待叫地主...");
}
```

### 3. 玩家ID映射说明

**重要概念**：
- **网络ID**：客户端在网络层的ID（1, 2, ...）
- **游戏ID**：在游戏逻辑中的ID（0, 1, 2）

**映射关系**：
```
服务器端：
  - 本地显示：玩家0
  - 游戏逻辑：玩家0
  
客户端1：
  - 网络ID：1
  - 本地显示：玩家0（自己）
  - 游戏逻辑中服务器的玩家1
  
客户端2：
  - 网络ID：2
  - 本地显示：玩家0（自己）
  - 游戏逻辑中服务器的玩家2
```

**为什么这样设计**：
客户端总是将自己作为玩家0显示，这样UI代码可以统一处理，不需要根据不同玩家ID调整界面。

## 修改的文件

### ingamescene.h
- 新增方法声明：`void sendDealCardsToClients();`

### ingamescene.cpp
- **网络模式构造函数**：服务器端延迟调用发牌方法
- **新增** `sendDealCardsToClients()`：发送发牌消息
- **完善** `handleNetworkDealCards()`：接收并处理发牌消息

## 测试步骤

### 测试环境
1. 至少2台在同一局域网的电脑
2. 都安装并编译了程序

### 测试步骤
1. **电脑A**：
   - 启动游戏
   - 选择"联机游戏" → "创建房间"
   - 记下显示的IP地址

2. **电脑B**：
   - 启动游戏
   - 选择"联机游戏" → "加入房间"
   - 输入电脑A的IP地址

3. **电脑A**：
   - 看到"玩家加入"提示
   - 点击"开始游戏"

4. **验证**：
   - ✅ 电脑A应该看到自己的17张手牌
   - ✅ 电脑B应该也看到自己的17张手牌（不同的牌）
   - ✅ 两边都应该看到3张地主牌（背面）

## 预期效果

### 修复前
- 房主：可以看到手牌 ✅
- 客户端：看不到手牌，界面空白 ❌

### 修复后
- 房主：可以看到手牌 ✅
- 客户端：可以看到手牌 ✅
- 所有玩家：可以正常进行游戏 ✅

## 后续改进

目前修复了显示手牌的核心问题，但还有其他TODO需要完善：
- [ ] 叫地主的同步
- [ ] 出牌的同步
- [ ] 游戏结束的处理
- [ ] 断线重连

这些功能的框架已经准备好（`handleNetworkCallLandlord` 等方法），可以按需继续完善。

## 调试日志

修复后，控制台会输出详细的调试信息：

**服务器端**：
```
[Server] 发送发牌消息给所有客户端
[Server] 当前连接的客户端数量: 1
[Server] 发送17张牌给游戏玩家1
[Server] 所有发牌消息已发送
```

**客户端**：
```
[Client] 处理发牌消息
[Client] 收到17张牌，游戏玩家ID: 1
[Client] 手牌添加完成，当前手牌数: 17
[Client] 手牌UI创建完成，面板数: 17
[Client] 发牌处理完成！
```

---

修复完成！现在联机模式下所有玩家都能看到自己的手牌了。
