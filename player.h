#ifndef PLAYER_H
#define PLAYER_H


#pragma once

#include <set>
#include <vector>
#include <string>
#include "cardgroup.h"

class Game;

/**
 * 玩家类型枚举
 */
enum PlayerType {
    LocalPlayer,    // 本地玩家（人类）
    NetworkPlayer,  // 网络玩家（其他联机玩家）
    AIPlayer        // AI玩家
};

/**
 * 玩家类
 * 管理玩家手牌、出牌逻辑、AI策略
 */
class Player {
    friend class Game;

public:
    Player(Game& game, int id, PlayerType type = AIPlayer);

    void NewGame();                    // 新游戏初始化
    void AddCard(int num);             // 添加一张牌到手牌
    int GetRemain() const;             // 获取剩余牌数
    int GetScore() const { return score; }
    void AddScore(int s) { score += s; }
    
    // 玩家类型相关
    PlayerType GetPlayerType() const { return playerType; }
    void SetPlayerType(PlayerType type) { playerType = type; }
    bool IsLocalPlayer() const { return playerType == LocalPlayer; }
    bool IsNetworkPlayer() const { return playerType == NetworkPlayer; }
    bool IsAIPlayer() const { return playerType == AIPlayer; }

    // 叫地主相关
    int CallLandlord(int questioned, int maxScore);

    // 出牌相关
    bool IsValid();                    // 判断选择的牌是否合法
    void AnalyseSelection();           // 分析选中的牌
    bool Discard();                    // AI出牌
    bool HumanDiscard();               // 真人出牌
    void Pass();                       // 过牌
    std::vector<CardGroup> Hint();     // 提示：返回所有可选方案

    // 选牌相关
    void SelectCards(const std::vector<int>& indices);  // 选牌
    void ClearSelection();             // 清空选牌

    const std::multiset<int>& GetCards() const { return cards; }
    std::vector<int> GetHandCards() const;  // 获取手牌向量（用于网络同步）
    void SetHandCards(const std::vector<int>& handCards);  // 设置手牌（用于网络同步）
    const CardGroup& GetSelection() const { return selection; }
    const CardGroup& GetLastDiscard() const { return discard; }

    int GetId() const { return playerId; }
    std::string GetName() const;

private:
    // AI逻辑
    void AISelectCards();
    std::vector<CardGroup> FindPlayableCards(const CardGroup& target) const; // 查找可出的所有方案
    void AnalyseCards();               // 分析手牌
    bool CanBeat(const CardGroup& target);  // 能否压过目标牌
    void OnSelectionChanged();         // 选牌变化时回调（分析缓存）

    Game& game;
    int playerId;                      // 玩家ID: 按加入顺序分配(0=房主, 1=第一个加入, 2=第二个加入或AI)
    PlayerType playerType;             // 玩家类型: LocalPlayer/NetworkPlayer/AIPlayer
    int score;                         // 玩家分数
    std::multiset<int> cards;          // 手牌（允许重复，例如有两张3）
    CardGroup selection;               // 选中的牌
    CardGroup discard;                 // 打出的牌
    std::vector<CardGroup> analyse;    // 牌型分析结果
    bool passed = false;               // 是否本轮已选择不出
};

#endif // PLAYER_H
