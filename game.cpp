#include "game.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <QDebug>

Game::Game() : status(NotStart), landlord(nullptr),
    curPlayer(nullptr), lastPlayer(nullptr),
    baseScore(0), multiple(1), questioned(0), callBegin(0),
    m_gameMode(GameMode::SinglePlayer), m_networkPlayerId(0) {
    for (int i = 0; i < 3; ++i) {
        players[i] = new Player(*this, i);
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

void Game::SetupNetworkPlayers(int localPlayerId, int totalPlayers) {
    // 在网络模式下，设置玩家类型和名称
    // localPlayerId 是本地玩家在网络游戏中的ID (0=服务器, 1,2=客户端)
    // totalPlayers 是总玩家数 (2或3)
    
    qDebug() << "[SetupNetworkPlayers] localPlayerId=" << localPlayerId << ", totalPlayers=" << totalPlayers;
    
    for (int i = 0; i < 3; ++i) {
        if (i == 0) {
            // 本地玩家总是以玩家0的身份存在
            players[i]->SetAI(false);
            players[i]->SetName("Player " + std::to_string(localPlayerId + 1));
            qDebug() << "  玩家" << i << ": 本地玩家 (Player " << (localPlayerId + 1) << ")";
        } else {
            // 计算实际网络玩家ID (从本地玩家的视角看)
            int networkId = (localPlayerId + i) % 3;
            
            // 判断这个位置是否是AI
            // 当只有2个真人玩家时，networkId=2的位置是AI
            if (totalPlayers == 2 && networkId == 2) {
                players[i]->SetAI(true);
                players[i]->SetName("AI");
                qDebug() << "  玩家" << i << ": AI (网络ID=" << networkId << ")";
            } else if (networkId < totalPlayers) {
                // 其他位置是真人网络玩家
                players[i]->SetAI(false);
                players[i]->SetName("Player " + std::to_string(networkId + 1));
                qDebug() << "  玩家" << i << ": 网络玩家 (Player " << (networkId + 1) << ")";
            } else {
                // 理论上不应该到这里
                players[i]->SetAI(true);
                players[i]->SetName("AI");
                qDebug() << "  玩家" << i << ": AI (默认，networkId=" << networkId << ")";
            }
        }
    }
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

    // 如果当前玩家不是AI（人类或网络玩家）,等待输入,不在这里处理
    if (!curPlayer->IsAI()) {
        return;
    }

    // AI玩家自动叫地主
    if (curPlayer->IsAI()) {
        int maxScore = 0;
        for (int i = 0; i < questioned; ++i) {
            if (callScores[i] > maxScore) {
                maxScore = callScores[i];
            }
        }

        int score = curPlayer->CallLandlord(questioned, maxScore);
        callScores[questioned] = score;
        qDebug() << "[CallLandlordPhase] AI 玩家" << curPlayer->GetId() << "叫分:" << score;

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
        curPlayer = players[0];
    }

    if (curPlayer != players[0]) return;

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

    // AI玩家自动出牌
    if (curPlayer != players[0]) {
        bool discarded = curPlayer->Discard();
        if (discarded) {
            lastPlayer = curPlayer;

            // 检查是否赢了
            if (curPlayer->GetRemain() == 0) {
                GameOverPhase();
                return;
            }
        }

        curPlayer = NextPlayer();
    }
}

//20251109 修改了return的type以便于ui检测出牌逻辑
bool Game::PlayerDiscard(const std::vector<int>& indices) {
    if (status != Status::Discard) return false;
    if (!curPlayer) return false;
    if (curPlayer != players[0]) return false;

    players[0]->SelectCards(indices);

    if (!players[0]->HumanDiscard()) {
        return false; // 出牌失败
    }

    lastPlayer = players[0];

    // 是否胜利
    if (players[0]->GetRemain() == 0) {
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
    if (curPlayer != players[0]) return;
    if (lastPlayer == curPlayer) return;  // 不能过自己的牌

    players[0]->Pass();
    curPlayer = NextPlayer();
}

void Game::PlayerHint() {
    if (status != Status::Discard) return;

    // 确保 curPlayer 已设置
    if (!curPlayer) return;
    if (curPlayer != players[0]) return;

    players[0]->Hint();
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


