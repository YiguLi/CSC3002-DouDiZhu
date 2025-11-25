#ifndef INGAMESCENE_H
#define INGAMESCENE_H

#include <QDialog>
#include <QVector>
#include <Qt>
#include <qlabel.h>

#include "game.h"
#include "cardpanel.h"
#include "networkmanager.h"

namespace Ui {
class InGameScene;
}

class InGameScene : public QDialog
{
    Q_OBJECT

public:
    // 单机模式构造函数
    explicit InGameScene(QWidget *parent = nullptr);
    // 网络模式构造函数
    explicit InGameScene(GameServer* server, GameClient* client, QWidget *parent = nullptr);
    ~InGameScene();

private:
    Ui::InGameScene *ui;

    Game m_game;                         // 当前这局游戏逻辑对象
    QVector<CardPanel*> m_handPanels;    // 玩家 0 手牌
    QVector<CardPanel*> m_landlordPanels;// 地主 3 张牌
    QVector<CardPanel*> m_lastPlayPanels[3];  // 0=玩家, 1=AI1, 2=AI2，每人一组出牌区

    QLabel* m_passLabels[3] = { nullptr, nullptr, nullptr }; // “不出”文字
    double m_lastPlayScale = 0.7;                             // 出牌区牌的缩放比例（比手牌小一点）


    // —— 初始化 UI & 游戏逻辑 ——
    void initGame();                     // 开局/重开一局（洗牌 + 发牌 + 进入叫地主阶段）
    void setupUIForCurrentGame();        // 不重新 GameStart，只根据当前 game 状态重画 UI
    void createLandlordPanels(bool faceUp);     // 显示地主牌（背面 / 正面）
    void createPlayer0HandPanels();             // 显示玩家 0 手牌
    void refreshPlayer0HandPanels();            // 刷新玩家 0 手牌布局
    void hideCallButtons();                     // 藏掉叫地主按钮
    void enterDiscardPhase();                   // 进入出牌阶段（UI 逻辑）
    void updateSelectedCards();
    void setStatusText(const QString &text);    //状态文字喵

    // —— UI：删除玩家刚刚出掉的牌 ——
    void applyPlayerDiscardToUI(const std::vector<int>& indices);

    // —— UI：显示“上家出的牌”在中间 ——
    void clearLastPlay();
    void showLastPlay();

    // 新增的工具函数
    void clearLastPlayForPlayer(int playerId);
    void showLastPlayForPlayer(Player* player);
    void showPassForPlayer(Player* player);
    QPoint getPlayAreaBasePos(int playerId) const;

    // —— UI：更新两个 AI 的剩余牌数标签 ——
    void updateAiRemainLabels();
    
    // —— 网络游戏相关 ——
    void initNetworkHandlers();
    void sendNetworkMessage(MessageType type, const QJsonObject& data);
    void sendDealCardsToClients(); // 服务器向客户端发送发牌消息
    
    // 网络消息处理
    void handleNetworkDealCards(const QJsonObject& data);
    void handleNetworkCallLandlord(const QJsonObject& data);
    void handleNetworkLandlordConfirm(const QJsonObject& data);
    void handleNetworkPlayerDiscard(const QJsonObject& data);
    void handleNetworkPlayerPass(const QJsonObject& data);
    void handleNetworkGameOver(const QJsonObject& data);

private slots:
    // —— 叫地主按钮 ——
    void onCallScore(int score);

    // —— 点击某张牌（选中 / 取消选中）——
    void onCardClicked();

    // —— 出牌按钮区域 ——
    void onPlayClicked();       // 出牌
    void onPassClicked();       // 过牌
    void onHintClicked();       // 提示（后面可以完善）
    
    // —— 网络消息槽 ——
    void onServerMessageReceived(int playerId, MessageType type, const QJsonObject& data);
    void onClientMessageReceived(MessageType type, const QJsonObject& data);

private:
    GameServer* m_server;       // 服务器对象（房主模式）
    GameClient* m_client;       // 客户端对象（客户端模式）
    bool m_isNetworkMode;       // 是否为网络模式
};


#endif // INGAMESCENE_H

