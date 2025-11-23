#include "ingamescene.h"
#include "ui_ingamescene.h"

#include <algorithm>
#include <QDebug>
#include <QTimer>
#include <QLabel>
#include <QFont>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>



#include "winscene.h"
#include "losescene.h"

InGameScene::InGameScene(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::InGameScene)
    , m_server(nullptr)
    , m_client(nullptr)
    , m_isNetworkMode(false)
{
    ui->setupUi(this);
    this->setWindowTitle("InGame");
    this->setFixedSize(800, 600);

    // 一开始先把出牌按钮隐藏（只在出牌阶段 / 玩家回合显示）
    ui->btn_play->hide();
    ui->btn_pass->hide();
    ui->btn_hint->hide();

    // 开一局游戏（洗牌 + 发牌 + 设置叫地主起始玩家）
    initGame();

    // 连接叫分按钮
    connect(ui->btn_notcall, &QPushButton::clicked, this, [this]() { onCallScore(0); });
    connect(ui->btn_1p,      &QPushButton::clicked, this, [this]() { onCallScore(1); });
    connect(ui->btn_2p,      &QPushButton::clicked, this, [this]() { onCallScore(2); });
    connect(ui->btn_3p,      &QPushButton::clicked, this, [this]() { onCallScore(3); });

    // 连接出牌区域按钮
    connect(ui->btn_play, &QPushButton::clicked, this, &InGameScene::onPlayClicked);
    connect(ui->btn_pass, &QPushButton::clicked, this, &InGameScene::onPassClicked);
    connect(ui->btn_hint, &QPushButton::clicked, this, &InGameScene::onHintClicked);
    qDebug() << "我是新版本";
}

// 网络模式构造函数
InGameScene::InGameScene(GameServer* server, GameClient* client, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::InGameScene)
    , m_server(server)
    , m_client(client)
    , m_isNetworkMode(true)
{
    ui->setupUi(this);
    this->setWindowTitle(server ? "InGame (房主)" : "InGame (客户端)");
    this->setFixedSize(800, 600);

    // 设置游戏为网络模式
    m_game.SetGameMode(GameMode::MultiPlayer);
    
    if (m_server) {
        m_game.SetNetworkPlayerId(0); // 服务器是玩家0
    } else if (m_client) {
        m_game.SetNetworkPlayerId(m_client->getMyPlayerId());
    }

    // 一开始先把出牌按钮隐藏
    ui->btn_play->hide();
    ui->btn_pass->hide();
    ui->btn_hint->hide();

    // 初始化网络消息处理
    initNetworkHandlers();

    // 如果是服务器，开始游戏
    if (m_server) {
        initGame();
    } else {
        // 客户端等待服务器发牌
        setStatusText("等待服务器开始游戏...");
    }

    // 连接叫分按钮
    connect(ui->btn_notcall, &QPushButton::clicked, this, [this]() { onCallScore(0); });
    connect(ui->btn_1p,      &QPushButton::clicked, this, [this]() { onCallScore(1); });
    connect(ui->btn_2p,      &QPushButton::clicked, this, [this]() { onCallScore(2); });
    connect(ui->btn_3p,      &QPushButton::clicked, this, [this]() { onCallScore(3); });

    // 连接出牌区域按钮
    connect(ui->btn_play, &QPushButton::clicked, this, &InGameScene::onPlayClicked);
    connect(ui->btn_pass, &QPushButton::clicked, this, &InGameScene::onPassClicked);
    connect(ui->btn_hint, &QPushButton::clicked, this, &InGameScene::onHintClicked);
}

InGameScene::~InGameScene()
{
    delete ui;
}

// —— 完整开局 / 重开一局 ——
void InGameScene::initGame()
{
    // 清空旧 UI
    clearLastPlay();
    qDeleteAll(m_handPanels);
    qDeleteAll(m_landlordPanels);
    m_handPanels.clear();
    m_landlordPanels.clear();
    setStatusText("Status: New Game Start");

    // 重新开局：洗牌 + 发牌 + 设置叫地主起始玩家
    qDebug() << "===== GameStart() 新一局 =====";
    m_game.GameStart();

    // 根据当前 game 状态重建 UI
    setupUIForCurrentGame();
}

// —— 只根据现有 game 状态重画 UI，不再 GameStart ——
void InGameScene::setupUIForCurrentGame()
{
    // 1. 画出上方 3 张地主牌（先背面）
    createLandlordPanels(false);

    // 2. 画出玩家 0 的手牌（正面）
    createPlayer0HandPanels();

    // 3. 更新 AI 剩余牌数（初始都是 17）
    updateAiRemainLabels();

    // 4. 让 AI 先叫到轮到玩家 0 为止
    while (m_game.GetStatus() == Status::GetLandlord &&
           m_game.GetCurrentPlayer() &&
           m_game.GetCurrentPlayer()->GetId() != 0)
    {
        qDebug() << "[init] AI" << m_game.GetCurrentPlayer()->GetId() << "正在叫分...";
        setStatusText("Status: AI calling...");
        m_game.CallLandlordPhase();

        // 如果 AI 叫分后已经确定地主，直接进入发地主牌阶段
        if (m_game.GetStatus() == Status::SendLandlordCard)
        {
            qDebug() << "[init] AI 已决定地主，发地主牌";
            setStatusText("Status: Game start!");
            m_game.SendLandlordCard();
            createLandlordPanels(true);
            refreshPlayer0HandPanels();
            hideCallButtons();
            enterDiscardPhase();
            return;
        }
    }

    // 5. 如果轮到玩家 0，显示叫分按钮
    if (m_game.GetStatus() == Status::GetLandlord &&
        m_game.GetCurrentPlayer() &&
        m_game.GetCurrentPlayer()->GetId() == 0)
    {
        qDebug() << "[setupUIForCurrentGame] 轮到玩家 0 叫分";
        setStatusText("Status: You Call!");
        ui->btn_notcall->show();
        ui->btn_1p->show();
        ui->btn_2p->show();
        ui->btn_3p->show();
    }
}

void InGameScene::createLandlordPanels(bool faceUp)
{
    // 清空旧的
    qDeleteAll(m_landlordPanels);
    m_landlordPanels.clear();

    int topY    = 60;
    int centerX = width() / 2;
    int spacing = 40;
    const double scale = 0.8;
    // 基准尺寸
    const int baseCardW = 70;
    const int baseCardH = 105;

    // 缩小后的尺寸
    const int cardW = static_cast<int>(baseCardW * scale);
    const int cardH = static_cast<int>(baseCardH * scale);

    for (int i = 0; i < 3; ++i)
    {
        int id = m_game.GetLandlordCard(i);

        auto *p = new CardPanel(this);
        p->setCardId(id);
        p->setFaceUp(faceUp);

        // 设置缩小后的尺寸
        p->setFixedSize(cardW, cardH);

        // 调整位置
        int x = centerX + (i - 1) * spacing - cardW / 2;
        p->move(x, topY);
        p->show();

        m_landlordPanels.append(p);
    }
}

void InGameScene::createPlayer0HandPanels()
{
    qDeleteAll(m_handPanels);
    m_handPanels.clear();

    Player *human = m_game.GetPlayer(0);
    const std::multiset<int> &cards = human->GetCards();

    std::vector<int> sorted(cards.begin(), cards.end());
    std::sort(sorted.begin(), sorted.end(), [](int a, int b) {
        return CardGroup::Translate(a) < CardGroup::Translate(b);
    });

    int spacing    = 35;
    int leftMargin = 30;
    int baseY      = height() - 150;

    for (int i = 0; i < (int)sorted.size(); ++i)
    {
        int id = sorted[i];

        auto *p = new CardPanel(this);
        p->setCardId(id);
        p->setFaceUp(true);

        p->move(leftMargin + i * spacing, baseY);
        p->show();

        connect(p, &CardPanel::clicked, this, &InGameScene::onCardClicked);

        m_handPanels.append(p);
    }
}

void InGameScene::refreshPlayer0HandPanels()
{
    createPlayer0HandPanels();
}

void InGameScene::hideCallButtons(){
    ui->btn_notcall->hide();
    ui->btn_1p->hide();
    ui->btn_2p->hide();
    ui->btn_3p->hide();
}

void InGameScene::onCallScore(int score) {
    qDebug() << "[onCallScore] 玩家点击叫分按钮，分数:" << score;

    m_game.PlayerCallLandlord(score);   // 只调这一句

    hideCallButtons();

    if (m_game.GetStatus() == Status::SendLandlordCard) {
        qDebug() << "-- 地主已决定 --";
        m_game.SendLandlordCard();
        createLandlordPanels(true);
        refreshPlayer0HandPanels();
        enterDiscardPhase();
        return;
    }

    if (m_game.GetStatus() == Status::GetLandlord &&
        m_game.GetLandlord() == nullptr)
    {
        qDebug() << "【无人叫地主】重新开局";
        initGame();
        return;
    }
}


void InGameScene::enterDiscardPhase()
{
    // ================== 先检查是否已经 GameOver ==================
    if (m_game.GetStatus() == Status::GameOver)
    {
        qDebug() << "游戏结束，弹出结果界面";

        Player* human = m_game.GetPlayer(0);
        bool humanWin = (human && human->GetRemain() == 0);

        // 禁用 / 隐藏出牌按钮，防止继续操作
        ui->btn_play->setEnabled(false);
        ui->btn_pass->setEnabled(false);
        ui->btn_hint->setEnabled(false);
        ui->btn_play->hide();
        ui->btn_pass->hide();
        ui->btn_hint->hide();

        if (humanWin) {
            auto *dlg = new WinScene(this);
            dlg->show();
        } else {

            auto *dlg = new LoseScene(this);
            dlg->show();
        }

        return;    // 已经结束，不再进入出牌阶段
    }
    // ================== 正常出牌阶段 ==================
    if (m_game.GetStatus() != Status::Discard)
        return;

    // 每次进阶段都刷新一下 AI 剩余牌数
    updateAiRemainLabels();

    Player* cur = m_game.GetCurrentPlayer();
    if (!cur) {
        qDebug() << "当前玩家为空，状态:" << (int)m_game.GetStatus();
        return;
    }

    // 玩家回合
    if (cur->GetId() == 0)
    {
        qDebug() << "轮到玩家出牌";
        setStatusText("Status: Your Turn!");
        ui->btn_play->show();
        ui->btn_pass->show();
        ui->btn_hint->show();
        ui->btn_play->setEnabled(true);
        ui->btn_pass->setEnabled(true);
        ui->btn_hint->setEnabled(true);
        return;
    }

    // AI 回合（加 2 秒延迟）
    qDebug() << "轮到 AI" << cur->GetId() << " 出牌，2 秒后执行";
    setStatusText(QString("Status： AI%1 Play…").arg(cur->GetId()));
    ui->btn_play->hide();
    ui->btn_pass->hide();
    ui->btn_hint->hide();

    Player* curAI = m_game.GetCurrentPlayer();
    int aiId = curAI->GetId();

    QTimer::singleShot(1500, this, [this, curAI, aiId]() {
        if (m_game.GetStatus() != Status::Discard) return;

        // 调用前后的 lastPlayer 用来判断 AI 是出牌还是过牌
        Player* beforeLast = m_game.GetLastPlayer();

        m_game.DiscardPhase(); // 让当前 AI 自动决定出牌或过牌

        Player* newLast = m_game.GetLastPlayer();
        const CardGroup& grp = m_game.GetLastDiscard();

        if (newLast == curAI && grp.GetCount() > 0) {
            // 说明这次是 curAI 出牌了
            showLastPlayForPlayer(curAI);
        } else {
            // lastPlayer 没变，或者没有出任何牌 => 认为是“过”
            showPassForPlayer(curAI);
        }

        updateAiRemainLabels();
        enterDiscardPhase();
    });
}



void InGameScene::onCardClicked()
{
    CardPanel *panel = qobject_cast<CardPanel*>(sender());
    if (!panel) return;

    bool sel = !panel->isSelected();
    panel->setSelected(sel);

    int dy = sel ? -20 : 20;
    panel->move(panel->x(), panel->y() + dy);
}

// —— UI 删除玩家刚刚出掉的牌 ——
void InGameScene::applyPlayerDiscardToUI(const std::vector<int>& indices)
{
    // 必须从大到小删，否则前面的删了下标会变
    std::vector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());

    for (int idx : sorted)
    {
        if (idx < 0 || idx >= m_handPanels.size()) continue;
        CardPanel* p = m_handPanels[idx];
        m_handPanels.remove(idx);
        p->deleteLater();
    }

    // 重新排版剩余的牌
    int spacing    = 30;
    int leftMargin = 40;
    int baseY      = height() - 150;

    for (int i = 0; i < m_handPanels.size(); ++i)
    {
        m_handPanels[i]->move(leftMargin + i * spacing, baseY);
    }
}

// —— 清空“上家出的牌”的显示 ——
// 清空指定玩家的出牌槽
void InGameScene::clearLastPlayForPlayer(int playerId)
{
    if (playerId < 0 || playerId >= 3) return;
    qDeleteAll(m_lastPlayPanels[playerId]);
    m_lastPlayPanels[playerId].clear();
}

// 清空所有玩家的出牌槽 + “不出”提示
void InGameScene::clearLastPlay()
{
    for (int i = 0; i < 3; ++i)
    {
        clearLastPlayForPlayer(i);
        if (m_passLabels[i]) {
            m_passLabels[i]->hide();
        }
    }
}

// 计算每个玩家出牌区域的中心点位置
// 计算每个玩家出牌区域的中心点位置
// 注意：这里的坐标要和 showLastPlayForPlayer 里牌的摆放保持一致，
// 这样“不出”就会正好出现在你现在出牌区域的位置上。
QPoint InGameScene::getPlayAreaBasePos(int playerId) const
{
    int w = width();
    int h = height();

    // 和 showLastPlayForPlayer 里保持同一套尺寸参数
    const int baseCardW   = 70;
    const int baseCardH   = 105;
    const double scale    = m_lastPlayScale;               // 出牌区缩放比例
    const int cardW       = static_cast<int>(baseCardW * scale);
    const int cardH       = static_cast<int>(baseCardH * scale);

    switch (playerId) {
    case 0: // 自己：整排居中，按钮上方一点
        // showLastPlayForPlayer 里：y = h - 260 - cardH/2，所以中心 y 就是 h - 260
        // x 居中，所以中心 x = w / 2
        return QPoint(w / 2, h - 260);

    case 1: // AI1：左侧，第一张牌左边距 40
        // showLastPlayForPlayer：startX = 40, y = h/2 - cardH/2 - 80
        // 一张牌的中心 ≈ (40 + cardW/2, h/2 - 80)
        return QPoint(40 + cardW / 2, h / 2 - 80);

    case 2: // AI2：右侧，最后一张牌右边距 40
        // showLastPlayForPlayer：startX = w - 40 - groupWidth
        // 近似用“最后一张牌中心”：(w - 40 - cardW/2, h/2 - 80)
        return QPoint(w - 40 - cardW / 2, h / 2 - 80);

    default:
        return QPoint(w / 2, h / 2);
    }
}



void InGameScene::showLastPlayForPlayer(Player* player)
{
    if (!player) return;

    int pid = player->GetId();
    if (pid < 0 || pid >= 3) return;

    // 清掉这个玩家之前的出牌 & “不出”
    clearLastPlayForPlayer(pid);
    if (m_passLabels[pid]) {
        m_passLabels[pid]->hide();
    }

    const CardGroup& grp = player->GetLastDiscard();
    if (grp.GetCount() == 0) {
        qDebug() << "showLastPlayForPlayer: player" << pid << " 出牌数 = 0";
        return;
    }

    // 拿到这次出的所有牌，并按点数从小到大排一下，方便排版
    std::vector<int> cards(grp.GetCards().begin(), grp.GetCards().end());
    std::sort(cards.begin(), cards.end(), [](int a, int b) {
        return CardGroup::Translate(a) < CardGroup::Translate(b);
    });

    int count = static_cast<int>(cards.size());
    if (count == 0) return;

    // 基准按手牌大小来：CardPanel 现在是 70x105
    const int baseCardW    = 70;
    const int baseCardH    = 105;
    const int baseSpacing  = 40;    // 牌之间的间距（你之前写的 40）

    // 出牌区缩放比例（在 InGameScene 里设置，比如 0.75）
    const double scale = m_lastPlayScale;   // 建议 m_lastPlayScale = 0.75;

    const int cardW   = static_cast<int>(baseCardW   * scale);
    const int cardH   = static_cast<int>(baseCardH   * scale);
    const int spacing = static_cast<int>(baseSpacing * scale);

    // 一整排的总宽度
    const int groupWidth = cardW + (count - 1) * spacing;

    const int w = width();
    const int h = height();

    int startX = 0;   // 第一张牌的左上角 x
    int y      = 0;   // 左上角 y

    switch (pid) {
    case 0: // 自己：整排居中，按钮上方一点
        startX = (w - groupWidth) / 2;
        y      = h - 260 - cardH / 2;
        break;
    case 1: // AI1：左侧，第一张牌左边距 40
        startX = 40;
        y      = h / 2 - cardH / 2 - 80;
        break;
    case 2: // AI2：右侧，最后一张牌右边距 40
        startX = w - 40 - groupWidth;
        y      = h / 2 - cardH / 2 - 80;
        break;
    default:
        startX = (w - groupWidth) / 2;
        y      = h / 2 - cardH / 2;
        break;
    }

    qDebug() << "在玩家" << pid << "旁边显示出牌, 张数:" << count
             << " 起点:" << startX << "," << y
             << " groupWidth:" << groupWidth
             << " cardSize:" << cardW << "x" << cardH;
    int offset       = 80;  // 飞进来的距离
    int delayPerCard = 40;  // 每张牌之间相差

    for (int i = 0; i < count; ++i)
    {
        CardPanel* p = new CardPanel(this);
        p->setCardId(cards[i]);
        p->setFaceUp(true);
        p->setSelected(false);
        p->setEnabled(false); // 展示，不可点

        p->setFixedSize(cardW, cardH);

        // 你原来算好的“最终位置”
        QRect endRect(
            startX + i * spacing, // x
            y,                    // y
            cardW,
            cardH
            );

        // 起始位置：在最终位置的基础上挪一点，当作“飞进来的起点”
        QRect startRect = endRect;
        if (pid == 0) {
            startRect.translate(0, offset);        // 自己：从下面往上飞
        } else if (pid == 1) {
            startRect.translate(-offset, 0);       // 左边 AI：从左往右飞
        } else if (pid == 2) {
            startRect.translate(offset, 0);        // 右边 AI：从右往左飞
        }

        p->setGeometry(startRect);
        p->show();

        // 为这一张牌创建一个动画
        auto *anim = new QPropertyAnimation(p, "geometry", this);
        anim->setDuration(250);                          // 单张牌动画时长
        anim->setStartValue(startRect);
        anim->setEndValue(endRect);
        anim->setEasingCurve(QEasingCurve::OutCubic);

        // 关键：按顺序延迟启动，让牌一张一张滑进来
        int delay = i * delayPerCard; // 第 i 张牌延迟 i*60 ms
        QTimer::singleShot(delay, this, [anim]() {
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        });

        m_lastPlayPanels[pid].append(p);
    }

}



// 根据 Game::GetLastPlayer / GetLastDiscard 决定给谁画
void InGameScene::showLastPlay()
{
    Player* last = m_game.GetLastPlayer();
    const CardGroup& grp = m_game.GetLastDiscard();

    if (!last) {
        qDebug() << "showLastPlay: lastPlayer 为 null";
        return;
    }
    if (grp.GetCount() == 0) {
        qDebug() << "showLastPlay: lastPlayer =" << last->GetId()
        << " 出牌数 = 0（可能是 pass）";
        return;
    }

    showLastPlayForPlayer(last);
}

// —— 更新两个 AI 的剩余牌数（使用你在 .ui 里放的 label） ——
void InGameScene::updateAiRemainLabels()
{
    if (!ui) return;

    Player* ai1 = m_game.GetPlayer(1);
    Player* ai2 = m_game.GetPlayer(2);
    if (!ai1 || !ai2) return;

    int r1 = ai1->GetRemain();
    int r2 = ai2->GetRemain();

    ui->label_ai1Remain->setText(QString("%1").arg(r1));
    ui->label_ai1Remain->setStyleSheet("color: white;");
    ui->label_ai2Remain->setText(QString("%1").arg(r2));
    ui->label_ai2Remain->setStyleSheet("color: white;");

    qDebug() << "updateAiRemainLabels: AI1 =" << r1 << ", AI2 =" << r2;
}

void InGameScene::onPlayClicked()
{
    // 收集当前选中的牌（UI 顺序索引）
    std::vector<int> indices;
    for (int i = 0; i < m_handPanels.size(); ++i) {
        if (m_handPanels[i]->isSelected())
            indices.push_back(i);
    }

    if (indices.empty()) {
        qDebug() << "没有选择牌";
        return;
    }

    bool ok = m_game.PlayerDiscard(indices);

    if (!ok) {
        qDebug() << "❌ 出牌失败（不合法或无法压上）";
        return;
    }

    // UI 删除出掉的牌
    applyPlayerDiscardToUI(indices);

    // 在玩家自己下方显示刚出的牌
    Player* last = m_game.GetLastPlayer();
    if (last && last->GetId() == 0) {
        showLastPlayForPlayer(last);
    }

    updateAiRemainLabels();

    // 轮到下一位（可能是 AI，也可能又轮到人）
    enterDiscardPhase();

}

void InGameScene::onPassClicked()
{
    qDebug() << "玩家选择过牌";

    Player* before = m_game.GetCurrentPlayer();

    m_game.PlayerPass();

    Player* after = m_game.GetCurrentPlayer();

    // 只有 curPlayer 真正交给了下一位，才认为 pass 成功
    if (before && before != after && before->GetId() == 0) {
        showPassForPlayer(before);
    }

    enterDiscardPhase();
}



void InGameScene::onHintClicked()
{
    qDebug() << "玩家请求提示";

    // 1. 先把当前已经弹起的牌全部放回去（清空 UI 选中状态）
    int baseY = height() - 150;   // 和 createPlayer0HandPanels / applyPlayerDiscardToUI 里保持一致
    for (CardPanel* panel : std::as_const(m_handPanels))
    {
        if (panel->isSelected())
        {
            panel->setSelected(false);
            panel->move(panel->x(), baseY);   // 往下放 20 像素
        }
    }

    // 2. 调用游戏逻辑生成提示（会把提示方案写进玩家的 selection）
    m_game.PlayerHint();

    // 3. 读取玩家当前的 selection
    Player* human = m_game.GetPlayer(0);
    if (!human) {
        qDebug() << "Hint: human player is null?";
        return;
    }

    const CardGroup& sel = human->GetSelection();
    const std::set<int>& hintCards = sel.GetCards();

    if (hintCards.empty()) {
        qDebug() << "Hint: 没有可以提示的牌（可能是压不上，建议过牌）";
        return;
    }

    // 4. 遍历手牌面板，凡是牌号在 hintCards 里的，就弹起来
    for (CardPanel* panel : std::as_const(m_handPanels))
    {
        // 这里用的是你 CardPanel 里的 cardId() 接口
        int id = panel->cardId();
        if (hintCards.find(id) != hintCards.end())
        {
            if (!panel->isSelected())
            {
                panel->setSelected(true);
                panel->move(panel->x(), panel->y() - 20);  // 往上弹 20 像素
            }
        }
    }
}

void InGameScene::setStatusText(const QString &text)
{
    ui->label_status->setText(text);
}

void InGameScene::showPassForPlayer(Player* player)
{
    if (!player) return;
    int pid = player->GetId();
    if (pid < 0 || pid >= 3) return;

    // 过牌时要清掉该玩家上一手出的牌
    clearLastPlayForPlayer(pid);

    // 第一次用的时候创建 QLabel
    if (!m_passLabels[pid]) {
        m_passLabels[pid] = new QLabel(this);
        m_passLabels[pid]->setText(QStringLiteral("不出"));

        QFont f = m_passLabels[pid]->font();
        f.setPointSize(14);
        f.setBold(true);
        m_passLabels[pid]->setFont(f);

        m_passLabels[pid]->setStyleSheet("color: red;");
        m_passLabels[pid]->setAlignment(Qt::AlignCenter);
        m_passLabels[pid]->setFixedSize(60, 30);
    }

    // 放到对应玩家的出牌区域中心
    QPoint center = getPlayAreaBasePos(pid);
    int x = center.x() - m_passLabels[pid]->width()  / 2;
    int y = center.y() - m_passLabels[pid]->height() / 2;

    m_passLabels[pid]->move(x, y);
    m_passLabels[pid]->show();
}

// ==================== 网络游戏相关方法 ====================

void InGameScene::initNetworkHandlers()
{
    if (m_server) {
        connect(m_server, &GameServer::messageReceived, 
                this, &InGameScene::onServerMessageReceived);
    }
    
    if (m_client) {
        connect(m_client, &GameClient::messageReceived,
                this, &InGameScene::onClientMessageReceived);
    }
}

void InGameScene::sendNetworkMessage(MessageType type, const QJsonObject& data)
{
    QJsonObject message;
    message["type"] = messageTypeToString(type);
    message["data"] = data;
    
    if (m_server) {
        m_server->broadcastMessage(message);
    } else if (m_client) {
        m_client->sendMessage(message);
    }
}

void InGameScene::onServerMessageReceived(int playerId, MessageType type, const QJsonObject& data)
{
    qDebug() << "[Server] 收到消息，玩家:" << playerId << "类型:" << messageTypeToString(type);
    
    // 服务器收到客户端消息后，广播给所有人
    switch (type) {
        case MessageType::CallLandlord:
            // 转发叫地主消息
            sendNetworkMessage(type, data);
            handleNetworkCallLandlord(data);
            break;
            
        case MessageType::PlayerDiscard:
            // 转发出牌消息
            sendNetworkMessage(type, data);
            handleNetworkPlayerDiscard(data);
            break;
            
        case MessageType::PlayerPass:
            // 转发过牌消息
            sendNetworkMessage(type, data);
            handleNetworkPlayerPass(data);
            break;
            
        default:
            break;
    }
}

void InGameScene::onClientMessageReceived(MessageType type, const QJsonObject& data)
{
    qDebug() << "[Client] 收到消息，类型:" << messageTypeToString(type);
    
    switch (type) {
        case MessageType::DealCards:
            handleNetworkDealCards(data);
            break;
            
        case MessageType::CallLandlord:
            handleNetworkCallLandlord(data);
            break;
            
        case MessageType::LandlordConfirm:
            handleNetworkLandlordConfirm(data);
            break;
            
        case MessageType::PlayerDiscard:
            handleNetworkPlayerDiscard(data);
            break;
            
        case MessageType::PlayerPass:
            handleNetworkPlayerPass(data);
            break;
            
        case MessageType::GameOver:
            handleNetworkGameOver(data);
            break;
            
        default:
            break;
    }
}

void InGameScene::handleNetworkDealCards(const QJsonObject& data)
{
    qDebug() << "[Network] 处理发牌消息";
    // 客户端收到服务器发牌
    // TODO: 实现客户端接收牌的逻辑
}

void InGameScene::handleNetworkCallLandlord(const QJsonObject& data)
{
    int playerId = data["playerId"].toInt();
    int score = data["score"].toInt();
    
    qDebug() << "[Network] 玩家" << playerId << "叫分:" << score;
    
    // TODO: 更新UI显示叫分结果
}

void InGameScene::handleNetworkLandlordConfirm(const QJsonObject& data)
{
    int landlordId = data["landlordId"].toInt();
    
    qDebug() << "[Network] 地主确定为玩家" << landlordId;
    
    // TODO: 更新UI显示地主
}

void InGameScene::handleNetworkPlayerDiscard(const QJsonObject& data)
{
    int playerId = data["playerId"].toInt();
    
    qDebug() << "[Network] 玩家" << playerId << "出牌";
    
    // TODO: 更新UI显示其他玩家出的牌
}

void InGameScene::handleNetworkPlayerPass(const QJsonObject& data)
{
    int playerId = data["playerId"].toInt();
    
    qDebug() << "[Network] 玩家" << playerId << "过牌";
    
    // TODO: 更新UI显示过牌
}

void InGameScene::handleNetworkGameOver(const QJsonObject& data)
{
    int winnerId = data["winnerId"].toInt();
    
    qDebug() << "[Network] 游戏结束，获胜者:" << winnerId;
    
    // TODO: 显示游戏结束界面
}
