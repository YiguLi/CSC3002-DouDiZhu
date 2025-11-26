#ifndef GAME_H
#define GAME_H

#include <string>
#include <QJsonObject>
#include "cards.h"
#include "player.h"

class NetworkManager;

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
    
    // 网络模式支持
    void SetNetworkMode(bool enabled) { isNetworkMode = enabled; }
    bool IsNetworkMode() const { return isNetworkMode; }
    void SetNetworkManager(NetworkManager* nm) { networkManager = nm; }
    NetworkManager* GetNetworkManager() const { return networkManager; }
    void SetupNetworkGame(int localPlayerId, bool useAI);  // 设置网络游戏
    void ApplyGameState(const QJsonObject& gameState);     // 应用游戏状态（客户端）
    
    // 网络游戏操作接收
    void OnNetworkCallLandlord(int playerId, int score);
    void OnNetworkDiscardCards(int playerId, const std::vector<int>& cards);
    void OnNetworkPass(int playerId);

    Status GetStatus() const { return status; }

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
    
    // 网络模式
    bool isNetworkMode;            // 是否为网络模式
    NetworkManager* networkManager; // 网络管理器
    int localPlayerId;             // 本地玩家ID
};


#endif // GAME_H
