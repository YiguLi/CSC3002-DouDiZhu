#include "game.h"
#include "networkmanager.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

Game::Game() : status(NotStart), landlord(nullptr),
    curPlayer(nullptr), lastPlayer(nullptr),
    baseScore(0), multiple(1), questioned(0), callBegin(0),
    isNetworkMode(false), networkManager(nullptr), localPlayerId(-1) {
    for (int i = 0; i < 3; ++i) {
        players[i] = new Player(*this, i, AIPlayer);  // 默认AI玩家
        callScores[i] = 0;
        landlordCards[i] = 0;
    }
}

Game::~Game() {
    for (int i = 0; i < 3; ++i) {
        delete players[i];
    }
}

void Game::InitGame() {
    landlord = nullptr;
    curPlayer = nullptr;
    lastPlayer = nullptr;
    baseScore = 0;
    multiple = 1;
    questioned = 0;
    callBegin = 0;

    for (int i = 0; i < 3; ++i) {
        players[i]->NewGame();
        callScores[i] = 0;
        landlordCards[i] = 0;
    }

    cardHeap.RandCards();
    status = Status::GetLandlord;  // 使用作用域解析
}

void Game::SendCard() {
    // 发牌: 每人17张,留3张作为地主牌
    while (cardHeap.GetRemain() > 3) {
        for (int i = 0; i < 3 && cardHeap.GetRemain() > 3; ++i) {
            int card = cardHeap.GetCard();
            players[i]->AddCard(card);
        }
    }

    // 记录地主牌
    for (int i = 0; i < 3; ++i) {
        landlordCards[i] = cardHeap.GetCard();
    }
}

void Game::GameStart() {
    InitGame();
    SendCard();

    // 初始化第一个叫地主的玩家
    callBegin = rand() % 3;
    curPlayer = players[callBegin];

    status = Status::GetLandlord;
}

void Game::LoadPlayerScore() {
    std::ifstream fin("data");
    if (fin.is_open()) {
        for (int i = 0; i < 3; ++i) {
            int score;
            if (fin >> score) {
                players[i]->score = score;
            }
        }
        fin.close();
    }
}

void Game::StorePlayerScore() {
    std::ofstream fout("data");
    if (fout.is_open()) {
        for (int i = 0; i < 3; ++i) {
            fout << players[i]->score << "\n";
        }
        fout.close();
    }
}

void Game::CallLandlordPhase() {
    if (status != Status::GetLandlord) return;

    // 确保 curPlayer 不为空(已在 GameStart 中初始化)
    if (!curPlayer) {
        curPlayer = players[callBegin];
    }

    // 如果当前玩家是本地玩家,等待输入
    if (curPlayer->IsLocalPlayer()) {
        return;
    }
    
    // 如果是网络玩家,等待网络消息
    if (curPlayer->IsNetworkPlayer()) {
        return;
    }

    // AI玩家自动叫地主
    if (curPlayer->IsAIPlayer()) {
        int maxScore = 0;
        for (int i = 0; i < questioned; ++i) {
            if (callScores[i] > maxScore) {
                maxScore = callScores[i];
            }
        }

        int score = curPlayer->CallLandlord(questioned, maxScore);
        callScores[questioned] = score;
        qDebug() << "[CallLandlordPhase] AI 玩家" << curPlayer->GetId() << "叫分:" << score;
        
        // 网络模式且是房主，需要广播AI叫地主
        if (isNetworkMode && networkManager && networkManager->IsHost()) {
            QJsonObject actionData;
            actionData["score"] = score;
            networkManager->SendAIAction(curPlayer->GetId(), MSG_CALL_LANDLORD, actionData);
            qDebug() << "[CallLandlordPhase] 广播AI玩家" << curPlayer->GetId() << "叫分:" << score;
        }

        if (score == 3) {
            landlord = curPlayer;
            baseScore = 3;
        }

        questioned++;

        if (questioned < 3 && !landlord) {
            curPlayer = NextPlayer();
        }
    }

    // 检查是否所有人都叫过了
    if (questioned == 3) {
        // 找出叫分最高的玩家
        int maxScore = 0;
        int maxIdx = -1;
        for (int i = 0; i < 3; ++i) {

            if (callScores[i] > maxScore) {
                maxScore = callScores[i];
                maxIdx = i;
            }
        }

        if (maxIdx != -1 && maxScore > 0) {
            landlord = players[maxIdx];
            baseScore = maxScore;
            status = Status::SendLandlordCard;
            qDebug() << "[CallLandlordPhase] 地主确定为玩家" << maxIdx << "，分数:" << maxScore;
        } else {
            // 没人叫地主,重新开始
            GameStart();
        }
    }
}

void Game::PlayerCallLandlord(int score) {
    qDebug() << "[PlayerCallLandlord] 玩家叫分函数被调用，分数:" << score;

    if (status != Status::GetLandlord) {
        qDebug() << "[PlayerCallLandlord] 状态错误，当前状态:" << (int)status;
        return;
    }

    // 确保 curPlayer 已设置
    if (!curPlayer) {
        curPlayer = players[localPlayerId >= 0 ? localPlayerId : 0];
    }

    if (!curPlayer->IsLocalPlayer()) return;
    
    // 网络模式下发送消息
    if (isNetworkMode && networkManager) {
        networkManager->SendCallLandlord(curPlayer->GetId(), score);
    }

    callScores[questioned] = score;

    if (score == 3) {
        landlord = curPlayer;
        baseScore = 3;
        status = Status::SendLandlordCard;
    } else {
        questioned++;
        if (questioned < 3) {
            curPlayer = NextPlayer();
            CallLandlordPhase();  // 继续AI叫地主
        } else {
            CallLandlordPhase();  // 处理结果
        }
    }
}

void Game::SendLandlordCard() {
    if (!landlord) return;

    // 把3张地主牌发给地主
    for (int i = 0; i < 3; ++i) {
        landlord->AddCard(landlordCards[i]);
    }

    // 检查是否有王或2,增加倍率
    for (int i = 0; i < 3; ++i) {
        int val = CardGroup::Translate(landlordCards[i]);
        if (val >= 15) multiple++;  // 2或王增加倍率
    }

    curPlayer = landlord;
    lastPlayer = nullptr;
    status = Status::Discard;
}

void Game::DiscardPhase() {
    if (status != Status::Discard) return;

    // 确保 curPlayer 不为空
    if (!curPlayer) return;
    
    // 本地玩家等待UI输入
    if (curPlayer->IsLocalPlayer()) {
        return;
    }
    
    // 网络玩家等待网络消息
    if (curPlayer->IsNetworkPlayer()) {
        return;
    }

    // AI玩家自动出牌
    if (curPlayer->IsAIPlayer()) {
        bool discarded = curPlayer->Discard();
        if (discarded) {
            lastPlayer = curPlayer;
            
            // 网络模式且是房主，需要广播AI操作
            if (isNetworkMode && networkManager && networkManager->IsHost()) {
                // 获取AI出的牌
                const CardGroup& aiDiscard = curPlayer->GetLastDiscard();
                std::vector<int> aiCards;
                for (int card : aiDiscard.cards) {
                    aiCards.push_back(card);
                }
                
                // 发送AI出牌消息
                QJsonObject actionData;
                QJsonArray cardArray;
                for (int card : aiCards) {
                    cardArray.append(card);
                }
                actionData["cards"] = cardArray;
                networkManager->SendAIAction(curPlayer->GetId(), MSG_DISCARD_CARDS, actionData);
                
                qDebug() << "[Game] 广播AI玩家" << curPlayer->GetId() << "出牌";
            }

            // 检查是否赢了
            if (curPlayer->GetRemain() == 0) {
                GameOverPhase();
                return;
            }
        } else {
            // AI选择不出（过牌）
            if (isNetworkMode && networkManager && networkManager->IsHost()) {
                QJsonObject actionData;
                networkManager->SendAIAction(curPlayer->GetId(), MSG_PASS, actionData);
                qDebug() << "[Game] 广播AI玩家" << curPlayer->GetId() << "过牌";
            }
        }

        curPlayer = NextPlayer();
    }
}

//20251109 修改了return的type以便于ui检测出牌逻辑
bool Game::PlayerDiscard(const std::vector<int>& indices) {
    if (status != Status::Discard) return false;
    if (!curPlayer) return false;
    if (!curPlayer->IsLocalPlayer()) return false;

    Player* localPlayer = players[localPlayerId >= 0 ? localPlayerId : 0];
    localPlayer->SelectCards(indices);

    if (!localPlayer->HumanDiscard()) {
        return false; // 出牌失败
    }
    
    // 网络模式下发送消息
    if (isNetworkMode && networkManager) {
        networkManager->SendDiscardCards(localPlayer->GetId(), indices);
    }

    lastPlayer = localPlayer;;

    // 是否胜利
    if (localPlayer->GetRemain() == 0) {
        GameOverPhase();
        return true;
    }

    // 只把轮次交给下一位，不在这里让 AI 出牌
    curPlayer = NextPlayer();
    return true;
}

void Game::PlayerPass() {
    if (status != Status::Discard) return;

    // 确保 curPlayer 已设置
    if (!curPlayer) return;
    if (!curPlayer->IsLocalPlayer()) return;
    if (lastPlayer == curPlayer) return;  // 不能过自己的牌

    Player* localPlayer = players[localPlayerId >= 0 ? localPlayerId : 0];
    
    // 网络模式下发送消息
    if (isNetworkMode && networkManager) {
        networkManager->SendPass(localPlayer->GetId());
    }
    
    localPlayer->Pass();
    curPlayer = NextPlayer();
}

void Game::PlayerHint() {
    if (status != Status::Discard) return;

    // 确保 curPlayer 已设置
    if (!curPlayer) return;
    if (!curPlayer->IsLocalPlayer()) return;

    Player* localPlayer = players[localPlayerId >= 0 ? localPlayerId : 0];
    localPlayer->Hint();
}

void Game::GameOverPhase() {
    status = Status::GameOver;

    // 计算分数
    int finalScore = baseScore * multiple;

    // 判断胜负
    Player* winner = nullptr;
    for (int i = 0; i < 3; ++i) {
        if (players[i]->GetRemain() == 0) {
            winner = players[i];
            break;
        }
    }

    if (!winner) return;

    bool landlordWin = (winner == landlord);

    if (landlordWin) {
        // 地主赢,农民扣分
        landlord->AddScore(finalScore * 2);
        for (int i = 0; i < 3; ++i) {
            if (players[i] != landlord) {
                players[i]->AddScore(-finalScore);
            }
        }
    } else {
        // 农民赢,地主扣分
        landlord->AddScore(-finalScore * 2);
        for (int i = 0; i < 3; ++i) {
            if (players[i] != landlord) {
                players[i]->AddScore(finalScore);
            }
        }
    }

    StorePlayerScore();
}

Player* Game::NextPlayer() {
    for (int i = 0; i < 3; ++i) {
        if (players[i] == curPlayer) {
            return players[(i + 1) % 3];
        }
    }
    return players[0];
}

Player* Game::PrevPlayer() {
    for (int i = 0; i < 3; ++i) {
        if (players[i] == curPlayer) {
            return players[(i + 2) % 3];
        }
    }
    return players[0];
}

const CardGroup& Game::GetLastDiscard() const {
    if (lastPlayer) {
        return lastPlayer->GetLastDiscard();
    }
    static CardGroup empty;
    return empty;
}

// 设置网络游戏
void Game::SetupNetworkGame(int localId, bool useAI) {
    localPlayerId = localId;
    isNetworkMode = true;
    
    qDebug() << "[Game] 设置网络游戏，本地玩家ID:" << localId << "，使用AI:" << useAI;
    
    // 设置玩家类型
    for (int i = 0; i < 3; ++i) {
        if (i == localId) {
            players[i]->SetPlayerType(LocalPlayer);
            qDebug() << "[Game] 玩家" << i << "设为本地玩家";
        } else if (useAI && i == 2) {
            players[i]->SetPlayerType(AIPlayer);
            qDebug() << "[Game] 玩家" << i << "设为AI玩家";
        } else {
            players[i]->SetPlayerType(NetworkPlayer);
            qDebug() << "[Game] 玩家" << i << "设为网络玩家";
        }
    }
}

// 处理网络叫地主消息
void Game::OnNetworkCallLandlord(int playerId, int score) {
    qDebug() << "[Game] 收到网络叫地主消息: 玩家" << playerId << "叫分" << score;
    
    if (status != Status::GetLandlord) return;
    if (playerId < 0 || playerId >= 3) return;
    
    callScores[questioned] = score;
    
    if (score == 3) {
        landlord = players[playerId];
        baseScore = 3;
        status = Status::SendLandlordCard;
    } else {
        questioned++;
        if (questioned < 3) {
            curPlayer = NextPlayer();
            CallLandlordPhase();
        } else {
            CallLandlordPhase();
        }
    }
}

// 处理网络出牌消息
void Game::OnNetworkDiscardCards(int playerId, const std::vector<int>& cards) {
    qDebug() << "[Game] 收到网络出牌消息: 玩家" << playerId << "出" << cards.size() << "张牌";
    
    if (status != Status::Discard) return;
    if (playerId < 0 || playerId >= 3) return;
    
    Player* player = players[playerId];
    if (!player) return;
    
    // 选择牌并出牌
    player->SelectCards(cards);
    if (player->HumanDiscard()) {
        lastPlayer = player;
        
        // 检查胜利
        if (player->GetRemain() == 0) {
            GameOverPhase();
            return;
        }
        
        curPlayer = NextPlayer();
    }
}

// 处理网络过牌消息
void Game::OnNetworkPass(int playerId) {
    qDebug() << "[Game] 收到网络过牌消息: 玩家" << playerId;
    
    if (status != Status::Discard) return;
    if (playerId < 0 || playerId >= 3) return;
    
    Player* player = players[playerId];
    if (!player) return;
    
    player->Pass();
    curPlayer = NextPlayer();
}


