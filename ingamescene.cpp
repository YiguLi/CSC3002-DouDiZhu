#include "ingamescene.h"

#include "ui_ingamescene.h"

#include "networkmanager.h"



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

    , m_gamePtr(new Game())

    , m_usingExternalGame(false)

{

    ui->setupUi(this);

    this->setWindowTitle("InGame");

    this->setFixedSize(800, 600);



    // 一开始先把出牌按钮隐藏藏（只在出牌阶段 / 玩家回合显示)?

    ui->btn_play->hide();

    ui->btn_pass->hide();

    ui->btn_hint->hide();



    // 开一局游戏（洗?+ 发牌 + 设置叫地主起始玩家)）

    initGame();



    // 连接叫分数按钮

    connect(ui->btn_notcall, &QPushButton::clicked, this, [this]() { onCallScore(0); });

    connect(ui->btn_1p,      &QPushButton::clicked, this, [this]() { onCallScore(1); });

    connect(ui->btn_2p,      &QPushButton::clicked, this, [this]() { onCallScore(2); });

    connect(ui->btn_3p,      &QPushButton::clicked, this, [this]() { onCallScore(3); });



    // 连接出牌区域按钮

    connect(ui->btn_play, &QPushButton::clicked, this, &InGameScene::onPlayClicked);

    connect(ui->btn_pass, &QPushButton::clicked, this, &InGameScene::onPassClicked);

    connect(ui->btn_hint, &QPushButton::clicked, this, &InGameScene::onHintClicked);

    qDebug() << "InGameScene created (single player mode)";

}



// 新增：使用外部Game实例的构造函数?

InGameScene::InGameScene(Game* externalGame, QWidget *parent)

    : QDialog(parent)

    , ui(new Ui::InGameScene)

    , m_gamePtr(externalGame)

    , m_usingExternalGame(true)

{

    ui->setupUi(this);

    this->setWindowTitle("InGame - Online");

    this->setFixedSize(800, 600);



    // 一开始先把出牌按钮隐藏?

    ui->btn_play->hide();

    ui->btn_pass->hide();

    ui->btn_hint->hide();



    // 不调用initGame，直接根据现有game状态态设置UI

    setupUIForCurrentGame();



    // 连接叫分数按钮

    connect(ui->btn_notcall, &QPushButton::clicked, this, [this]() { onCallScore(0); });

    connect(ui->btn_1p,      &QPushButton::clicked, this, [this]() { onCallScore(1); });

    connect(ui->btn_2p,      &QPushButton::clicked, this, [this]() { onCallScore(2); });

    connect(ui->btn_3p,      &QPushButton::clicked, this, [this]() { onCallScore(3); });



    // 连接出牌区域按钮

    connect(ui->btn_play, &QPushButton::clicked, this, &InGameScene::onPlayClicked);

    connect(ui->btn_pass, &QPushButton::clicked, this, &InGameScene::onPassClicked);

    connect(ui->btn_hint, &QPushButton::clicked, this, &InGameScene::onHintClicked);

    

    // 设置定时器定期检查游戏状态态（用于网络模式的UI更新）

    QTimer* updateTimer = new QTimer(this);

    connect(updateTimer, &QTimer::timeout, this, [this]() {

        // 检查是否需要更新UI

        updateAiRemainLabels();

        

        // 检查游戏是否结束

        if (m_gamePtr->GetStatus() == Status::GameOver) {

            int localPlayerId = 0;

            if (m_gamePtr->IsNetworkMode() && m_gamePtr->GetNetworkManager()) {

                localPlayerId = m_gamePtr->GetNetworkManager()->GetLocalPlayerId();

            }

            Player* human = m_gamePtr->GetPlayer(localPlayerId);

            if (human && human->GetRemain() == 0) {

                auto *dlg = new WinScene(this);

                dlg->show();

            } else {

                auto *dlg = new LoseScene(this);

                dlg->show();

            }

        }

    });

    updateTimer->start(500); // 每500ms检查一次

    

    qDebug() << "InGameScene创建完成（外部Game模式）";

}



InGameScene::~InGameScene()

{

    delete ui;

    if (!m_usingExternalGame && m_gamePtr) {

        delete m_gamePtr;

    }

}



// 设置外部Game实例

void InGameScene::setGame(Game* game)

{

    if (m_gamePtr && !m_usingExternalGame) {

        delete m_gamePtr;

    }

    m_gamePtr = game;

    m_usingExternalGame = true;

    setupUIForCurrentGame();

}



// —?完整开局 / 重开一局 —?

void InGameScene::initGame()

{

    // 清空?UI

    clearLastPlay();

    qDeleteAll(m_handPanels);

    qDeleteAll(m_landlordPanels);

    m_handPanels.clear();

    m_landlordPanels.clear();

    setStatusText("Status: New Game Start");



    // 重新开局：洗?+ 发牌 + 设置叫地主起始玩?

    qDebug() << "===== GameStart() 新一局 =====";

    m_gamePtr->GameStart();



    // 根据当前 game 状态态重?UI

    setupUIForCurrentGame();

}



// —?只根据现?game 状态态重?UI，不?GameStart —?

void InGameScene::setupUIForCurrentGame()

{

    // 1. 画出上方 3 张地主牌（先背面?

    createLandlordPanels(false);



    // 2. 画出玩家 0 的手牌（正面?

    createPlayer0HandPanels();



    // 3. 更新 AI 剩余牌数（初始都?17?

    updateAiRemainLabels();



    // 4. 让AI先叫到轮到玩家0为止（仅单机模式）

    if (!m_gamePtr->IsNetworkMode()) {

        while (m_gamePtr->GetStatus() == Status::GetLandlord &&

               m_gamePtr->GetCurrentPlayer() &&

               m_gamePtr->GetCurrentPlayer()->GetId() != 0)

        {

            qDebug() << "[init] AI" << m_gamePtr->GetCurrentPlayer()->GetId() << "正在叫分数...";

            setStatusText("Status: AI calling...");

            m_gamePtr->CallLandlordPhase();



            // 如果 AI 叫分数后已经确定地主，直接进入发地主牌阶段

            if (m_gamePtr->GetStatus() == Status::SendLandlordCard)

            {

                qDebug() << "[init] AI 已决定地主，发地主牌";

                setStatusText("Status: Game start!");

                m_gamePtr->SendLandlordCard();

                createLandlordPanels(true);

                refreshPlayer0HandPanels();

                hideCallButtons();

                enterDiscardPhase();

            return;

        }

    }

    }



    // 5. 如果轮到本地玩家，显示叫分数按钮

    if (m_gamePtr->GetStatus() == Status::GetLandlord &&

        m_gamePtr->GetCurrentPlayer())

    {

        Player* cur = m_gamePtr->GetCurrentPlayer();

        if (cur->IsLocalPlayer()) {

            qDebug() << "[setupUIForCurrentGame] 轮到本地玩家叫分数";

            setStatusText("Status: You Call!");

            ui->btn_notcall->show();

            ui->btn_1p->show();

            ui->btn_2p->show();

            ui->btn_3p->show();

        } else if (m_gamePtr->IsNetworkMode()) {

            // 网络模式下，非本地玩家的回合

            if (cur->IsAIPlayer()) {

                // AI玩家，房主负责执行

                if (m_gamePtr->GetNetworkManager() && m_gamePtr->GetNetworkManager()->IsHost()) {

                    qDebug() << "[setupUIForCurrentGame] 网络模式：AI" << cur->GetId() << "开始叫地主";

                    QTimer::singleShot(1000, this, [this]() {

                        if (m_gamePtr->GetStatus() == Status::GetLandlord) {

                            m_gamePtr->CallLandlordPhase();

                        }

                    });

                }

            } else {

                // 网络玩家，等待网络消息

                qDebug() << "[setupUIForCurrentGame] 等待网络玩家" << cur->GetId() << "叫地主";

                setStatusText(QString("Status: Player %1 calling...").arg(cur->GetId()));

            }

        }

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

        int id = m_gamePtr->GetLandlordCard(i);



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



    Player *human = m_gamePtr->GetPlayer(0);

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

    qDebug() << "[onCallScore] 玩家点击叫分数按钮，分数?" << score;



    m_gamePtr->PlayerCallLandlord(score);   // 只调这一?



    hideCallButtons();



    if (m_gamePtr->GetStatus() == Status::SendLandlordCard) {

        qDebug() << "-- 地主已决?--";

        m_gamePtr->SendLandlordCard();

        createLandlordPanels(true);

        refreshPlayer0HandPanels();

        enterDiscardPhase();

        return;

    }



    if (m_gamePtr->GetStatus() == Status::GetLandlord &&

        m_gamePtr->GetLandlord() == nullptr)

    {

        qDebug() << "【无人叫地主】重新开局";

        initGame();

        return;

    }

}





void InGameScene::enterDiscardPhase()

{

    // ================== 先检查是否已?GameOver ==================

    if (m_gamePtr->GetStatus() == Status::GameOver)

    {

        qDebug() << "Game Over, showing result scene";



        Player* human = m_gamePtr->GetPlayer(0);

        bool humanWin = (human && human->GetRemain() == 0);



        // 禁用 / 隐藏出牌按钮，防止继续操?

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



        return;    // 已经结束，不再进入出牌阶?

    }

    // ================== 正常出牌阶段 ==================

    if (m_gamePtr->GetStatus() != Status::Discard)

        return;



    // 每次进阶段都刷新一?AI 剩余牌数

    updateAiRemainLabels();



    Player* cur = m_gamePtr->GetCurrentPlayer();

    if (!cur) {

        qDebug() << "当前玩家为空，状态?" << (int)m_gamePtr->GetStatus();

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



    // AI turn (with 2 second delay)

    qDebug() << "AI" << cur->GetId() << " turn to play, execute after 2s";

    setStatusText(QString("Status: AI%1 Playing").arg(cur->GetId()));

    ui->btn_play->hide();

    ui->btn_pass->hide();

    ui->btn_hint->hide();



    Player* curAI = m_gamePtr->GetCurrentPlayer();

    int aiId = curAI->GetId();



    QTimer::singleShot(1500, this, [this, curAI, aiId]() {

        if (m_gamePtr->GetStatus() != Status::Discard) return;



        // 调用前后?lastPlayer 用来判断 AI 是出牌还是过?

        Player* beforeLast = m_gamePtr->GetLastPlayer();



        m_gamePtr->DiscardPhase(); // 让当?AI 自动决定出牌或过?



        Player* newLast = m_gamePtr->GetLastPlayer();

        const CardGroup& grp = m_gamePtr->GetLastDiscard();



        if (newLast == curAI && grp.GetCount() > 0) {

            // 说明这次?curAI 出牌?

            showLastPlayForPlayer(curAI);

        } else {

            // lastPlayer 没变，或者没有出任何?=> 认为是“过?

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



// —?UI 删除玩家刚刚出掉的牌 —?

void InGameScene::applyPlayerDiscardToUI(const std::vector<int>& indices)

{

    // 必须从大到小删，否则前面的删了下标会?

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



// —?清空“上家出的牌”的显示) —?

// 清空指定玩家的出牌槽

void InGameScene::clearLastPlayForPlayer(int playerId)

{

    if (playerId < 0 || playerId >= 3) return;

    qDeleteAll(m_lastPlayPanels[playerId]);

    m_lastPlayPanels[playerId].clear();

}



// 清空所有玩家的出牌?+ “不出”提?

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

// 这样“不出”就会正好出现在你现在出牌区域的位置上?

QPoint InGameScene::getPlayAreaBasePos(int playerId) const

{

    int w = width();

    int h = height();



    // ?showLastPlayForPlayer 里保持同一套尺寸参?

    const int baseCardW   = 70;

    const int baseCardH   = 105;

    const double scale    = m_lastPlayScale;               // 出牌区缩放比?

    const int cardW       = static_cast<int>(baseCardW * scale);

    const int cardH       = static_cast<int>(baseCardH * scale);



    switch (playerId) {

    case 0: // 自己：整排居中，按钮上方一?

        // showLastPlayForPlayer 里：y = h - 260 - cardH/2，所以中?y 就是 h - 260

        // x 居中，所以中?x = w / 2

        return QPoint(w / 2, h - 260);



    case 1: // AI1：左侧，第一张牌左边?40

        // showLastPlayForPlayer：startX = 40, y = h/2 - cardH/2 - 80

        // 一张牌的中??(40 + cardW/2, h/2 - 80)

        return QPoint(40 + cardW / 2, h / 2 - 80);



    case 2: // AI2：右侧，最后一张牌右边?40

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



    // 清掉这个玩家之前的出?& “不出?

    clearLastPlayForPlayer(pid);

    if (m_passLabels[pid]) {

        m_passLabels[pid]->hide();

    }



    const CardGroup& grp = player->GetLastDiscard();

    if (grp.GetCount() == 0) {

        qDebug() << "showLastPlayForPlayer: player" << pid << " 出牌?= 0";

        return;

    }



    // 拿到这次出的所有牌，并按点数从小到大排一下，方便排版

    std::vector<int> cards(grp.GetCards().begin(), grp.GetCards().end());

    std::sort(cards.begin(), cards.end(), [](int a, int b) {

        return CardGroup::Translate(a) < CardGroup::Translate(b);

    });



    int count = static_cast<int>(cards.size());

    if (count == 0) return;



    // 基准按手牌大小来：CardPanel 现在?70x105

    const int baseCardW    = 70;

    const int baseCardH    = 105;

    const int baseSpacing  = 40;    // 牌之间的间距（你之前写的 40?



    // 出牌区缩放比例（?InGameScene 里设置，比如 0.75?

    const double scale = m_lastPlayScale;   // 建议 m_lastPlayScale = 0.75;



    const int cardW   = static_cast<int>(baseCardW   * scale);

    const int cardH   = static_cast<int>(baseCardH   * scale);

    const int spacing = static_cast<int>(baseSpacing * scale);



    // 一整排的总宽?

    const int groupWidth = cardW + (count - 1) * spacing;



    const int w = width();

    const int h = height();



    int startX = 0;   // 第一张牌的左上角 x

    int y      = 0;   // 左上?y



    switch (pid) {

    case 0: // 自己：整排居中，按钮上方一?

        startX = (w - groupWidth) / 2;

        y      = h - 260 - cardH / 2;

        break;

    case 1: // AI1：左侧，第一张牌左边?40

        startX = 40;

        y      = h / 2 - cardH / 2 - 80;

        break;

    case 2: // AI2：右侧，最后一张牌右边?40

        startX = w - 40 - groupWidth;

        y      = h / 2 - cardH / 2 - 80;

        break;

    default:

        startX = (w - groupWidth) / 2;

        y      = h / 2 - cardH / 2;

        break;

    }



    qDebug() << "Showing cards for player" << pid << "count:" << count

             << " start:" << startX << "," << y

             << " groupWidth:" << groupWidth

             << " cardSize:" << cardW << "x" << cardH;

    int offset       = 80;  // 飞进来的距离

    int delayPerCard = 40;  // 每张牌之间相?



    for (int i = 0; i < count; ++i)

    {

        CardPanel* p = new CardPanel(this);

        p->setCardId(cards[i]);

        p->setFaceUp(true);

        p->setSelected(false);

        p->setEnabled(false); // 展示，不可点



        p->setFixedSize(cardW, cardH);



        // 你原来算好的“最终位置?

        QRect endRect(

            startX + i * spacing, // x

            y,                    // y

            cardW,

            cardH

            );



        // 起始位置：在最终位置的基础上挪一点，当作“飞进来的起点?

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



        // 为这一张牌创建一个动?

        auto *anim = new QPropertyAnimation(p, "geometry", this);

        anim->setDuration(250);                          // 单张牌动画时?

        anim->setStartValue(startRect);

        anim->setEndValue(endRect);

        anim->setEasingCurve(QEasingCurve::OutCubic);



        // 关键：按顺序延迟启动，让牌一张一张滑进来

        int delay = i * delayPerCard; // ?i 张牌延迟 i*60 ms

        QTimer::singleShot(delay, this, [anim]() {

            anim->start(QAbstractAnimation::DeleteWhenStopped);

        });



        m_lastPlayPanels[pid].append(p);

    }



}







// 根据 Game::GetLastPlayer / GetLastDiscard 决定给谁?

void InGameScene::showLastPlay()

{

    Player* last = m_gamePtr->GetLastPlayer();

    const CardGroup& grp = m_gamePtr->GetLastDiscard();



    if (!last) {

        qDebug() << "showLastPlay: lastPlayer ?null";

        return;

    }

    if (grp.GetCount() == 0) {

        qDebug() << "showLastPlay: lastPlayer =" << last->GetId()

        << " discard count = 0 (might be pass)";

        return;

    }



    showLastPlayForPlayer(last);

}



// —?更新两个 AI 的剩余牌数（使用你在 .ui 里放?label?—?

void InGameScene::updateAiRemainLabels()

{

    if (!ui) return;



    Player* ai1 = m_gamePtr->GetPlayer(1);

    Player* ai2 = m_gamePtr->GetPlayer(2);

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

    // 收集当前选中的牌（UI 顺序索引?

    std::vector<int> indices;

    for (int i = 0; i < m_handPanels.size(); ++i) {

        if (m_handPanels[i]->isSelected())

            indices.push_back(i);

    }



    if (indices.empty()) {

        qDebug() << "No cards selected";

        return;

    }



    bool ok = m_gamePtr->PlayerDiscard(indices);



    if (!ok) {

        qDebug() << "?出牌失败（不合法或无法压上）";

        return;

    }



    // UI 删除出掉的牌

    applyPlayerDiscardToUI(indices);



    // 在玩家自己下方显示)刚出的?

    Player* last = m_gamePtr->GetLastPlayer();

    if (last && last->GetId() == 0) {

        showLastPlayForPlayer(last);

    }



    updateAiRemainLabels();



    // 轮到下一位（可能?AI，也可能又轮到人?

    enterDiscardPhase();



}



void InGameScene::onPassClicked()

{

    qDebug() << "玩家选择过牌";



    Player* before = m_gamePtr->GetCurrentPlayer();



    m_gamePtr->PlayerPass();



    Player* after = m_gamePtr->GetCurrentPlayer();



    // 只有 curPlayer 真正交给了下一位，才认?pass 成功

    if (before && before != after && before->GetId() == 0) {

        showPassForPlayer(before);

    }



    enterDiscardPhase();

}







void InGameScene::onHintClicked()

{

    qDebug() << "玩家请求提示";



    // 1. 先把当前已经弹起的牌全部放回去（清空 UI 选中状态态）

    int baseY = height() - 150;   // ?createPlayer0HandPanels / applyPlayerDiscardToUI 里保持一?

    for (CardPanel* panel : std::as_const(m_handPanels))

    {

        if (panel->isSelected())

        {

            panel->setSelected(false);

            panel->move(panel->x(), baseY);   // 往下放 20 像素

        }

    }



    // 2. 调用游戏逻辑生成提示（会把提示方案写进玩家的 selection?

    m_gamePtr->PlayerHint();



    // 3. 读取玩家当前?selection

    Player* human = m_gamePtr->GetPlayer(0);

    if (!human) {

        qDebug() << "Hint: No valid cards to suggest (recommend pass)";

        return;

    }



    const CardGroup& sel = human->GetSelection();

    const std::set<int>& hintCards = sel.GetCards();



    if (hintCards.empty()) {

        qDebug() << "Hint: No valid cards to suggest (recommend pass)";

        return;

    }



    // 4. 遍历手牌面板，凡是牌号在 hintCards 里的，就弹起?

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



    // 第一次用的时候创?QLabel

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



    // 放到对应玩家的出牌区域中?

    QPoint center = getPlayAreaBasePos(pid);

    int x = center.x() - m_passLabels[pid]->width()  / 2;

    int y = center.y() - m_passLabels[pid]->height() / 2;



    m_passLabels[pid]->move(x, y);

    m_passLabels[pid]->show();

}













