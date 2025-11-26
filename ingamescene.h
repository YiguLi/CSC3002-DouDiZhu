#ifndef INGAMESCENE_H
#define INGAMESCENE_H

#include <QDialog>
#include <QVector>
#include <Qt>
#include <qlabel.h>

#include "game.h"
#include "cardpanel.h"

namespace Ui {
class InGameScene;
}

class InGameScene : public QDialog
{
    Q_OBJECT

public:
    explicit InGameScene(QWidget *parent = nullptr);
    explicit InGameScene(Game* externalGame, QWidget *parent = nullptr); // 新增：使用外部Game
    ~InGameScene();
    
    void setGame(Game* game); // 设置外部Game实例

private:
    Ui::InGameScene *ui;

    Game* m_gamePtr;                     // 改为指针
    bool m_usingExternalGame;            // 标记是否使用外部Game
    QVector<CardPanel*> m_handPanels;    // 玩家 0 手牌
    QVector<CardPanel*> m_landlordPanels;// 地主 3 张牌
    QVector<CardPanel*> m_lastPlayPanels[3];  // 0=玩家, 1=AI1, 2=AI2，每人一组出牌区

    QLabel* m_passLabels[3] = { nullptr, nullptr, nullptr }; // "不出"文字
    double m_lastPlayScale = 0.7;                             // 出牌区牌的缩放比例（比手牌小一点）

    // 用于减少日志输出的缓存
    int m_lastAI1Remain = -1;
    int m_lastAI2Remain = -1;

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
    
    // —— 辅助方法：获取Game引用 ——
    Game& game() { return *m_gamePtr; }

private slots:
    // —— 叫地主按钮 ——
    void onCallScore(int score);

    // —— 点击某张牌（选中 / 取消选中）——
    void onCardClicked();

    // —— 出牌按钮区域 ——
    void onPlayClicked();       // 出牌
    void onPassClicked();       // 过牌
    void onHintClicked();       // 提示（后面可以完善）
};


#endif // INGAMESCENE_H

