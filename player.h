#ifndef PLAYER_H
#define PLAYER_H


#pragma once

#include <set>
#include <vector>
#include <string>
#include "cardgroup.h"

class Game;

/**
 * 玩家类
 * 管理玩家手牌、出牌逻辑、AI策略
 */
class Player {
    friend class Game;

public:
    Player(Game& game, int id);

    void NewGame();                    // 新游戏初始化
    void AddCard(int num);             // 添加一张牌到手牌
    int GetRemain() const;             // 获取剩余牌数
    int GetScore() const { return score; }
    void AddScore(int s) { score += s; }
    
    // 玩家类型和名称
    bool IsAI() const { return isAI; }
    void SetAI(bool ai) { isAI = ai; }
    void SetName(const std::string& name) { playerName = name; }

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
    int playerId;                      // 玩家ID: 0=人类, 1=电脑1, 2=电脑2
    int score;                         // 玩家分数
    bool isAI;                         // 是否为AI（单机模式下ID>0为AI，联机模式根据网络身份判断）
    std::string playerName;            // 玩家名称
    std::multiset<int> cards;          // 手牌（允许重复，例如有两张3）
    CardGroup selection;               // 选中的牌
    CardGroup discard;                 // 打出的牌
    std::vector<CardGroup> analyse;    // 牌型分析结果
    bool passed = false;               // 是否本轮已选择不出
};

#endif // PLAYER_H
