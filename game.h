#ifndef GAME_H
#define GAME_H

#include <string>
#include "cards.h"
#include "player.h"

/**
 * 游戏状态枚举
 */
enum Status {
    NotStart,           // 游戏未开始
    GetLandlord,        // 叫地主阶段
    SendLandlordCard,   // 发地主牌阶段
    Discard,            // 出牌阶段
    GameOver            // 游戏结束
};

/**
 * 游戏模式枚举
 */
enum class GameMode {
    SinglePlayer,       // 单机模式（1人+2AI）
    MultiPlayer         // 联机模式（2-3真人）
};

/**
 * 游戏主控制类
 * 管理整个游戏流程
 */
class Game {
    friend class Player;

public:
    Game();
    ~Game();

    void GameStart();              // 开始新游戏
    void InitGame();               // 初始化游戏
    void LoadPlayerScore();        // 加载玩家分数
    void StorePlayerScore();       // 保存玩家分数

    Status GetStatus() const { return status; }
    
    // 网络模式相关
    void SetGameMode(GameMode mode) { m_gameMode = mode; }
    GameMode GetGameMode() const { return m_gameMode; }
    bool IsNetworkMode() const { return m_gameMode == GameMode::MultiPlayer; }
    void SetNetworkPlayerId(int id) { m_networkPlayerId = id; } // 设置本地玩家在网络中的ID
    int GetNetworkPlayerId() const { return m_networkPlayerId; }

    // 游戏流程控制
    void CallLandlordPhase();      // 叫地主阶段
    void SendLandlordCard();       // 发地主牌
    void DiscardPhase();           // 出牌阶段
    void GameOverPhase();          // 游戏结束

    // 玩家操作
    void PlayerCallLandlord(int score);  // 玩家叫地主
    bool PlayerDiscard(const std::vector<int>& indices);  // 玩家出牌
    void PlayerPass();             // 玩家过牌
    void PlayerHint();             // 提示

    // 游戏信息查询
    Player* GetCurrentPlayer() { return curPlayer; }
    Player* GetLandlord() { return landlord; }
    Player* GetPlayer(int id) { return players[id]; }
    Player* GetLastPlayer() { return lastPlayer; }
    const CardGroup& GetLastDiscard() const;

    int GetBaseScore() const { return baseScore; }
    int GetMultiple() const { return multiple; }
    int GetLandlordCard(int index) const { return landlordCards[index]; }

    bool IsPlayerTurn() const { return curPlayer == players[0]; }

private:
    void SendCard();               // 发牌
    Player* NextPlayer();          // 获取下一个玩家
    Player* PrevPlayer();          // 获取上一个玩家

    Status status;                 // 游戏状态
    Cards cardHeap;                // 发牌堆
    Player* players[3];            // 三个玩家: [0]=人类, [1][2]=AI
    Player* landlord;              // 地主
    Player* curPlayer;             // 当前玩家
    Player* lastPlayer;            // 最后出牌的玩家

    int callScores[3];             // 各玩家叫地主的分数
    int callBegin;                 // 第一个叫地主的玩家
    int questioned;                // 已询问的玩家数
    int baseScore;                 // 本局基础分
    int multiple;                  // 倍率
    int landlordCards[3];          // 三张地主牌
    
    // 网络模式相关
    GameMode m_gameMode;           // 游戏模式
    int m_networkPlayerId;         // 本地玩家在网络游戏中的ID（0=服务器，1,2=客户端）
};


#endif // GAME_H
