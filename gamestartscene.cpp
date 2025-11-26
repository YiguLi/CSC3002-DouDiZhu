#include "gamestartscene.h"
#include "ui_gamestartscene.h"
#include "selectscene.h"
#include "lobbyroom.h"
#include "networkmanager.h"
#include "game.h"
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>

Gamestartscene::Gamestartscene(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Gamestartscene)
{
    ui->setupUi(this);
    this -> setWindowTitle("start");
    this->setFixedSize(800, 600);
}

Gamestartscene::~Gamestartscene()
{
    delete ui;
}
//进入选择界面
void Gamestartscene::on_start_clicked()
{
    SelectScene *w = new SelectScene();
    w -> show();
    this -> hide();
}


//进入联机模式
void Gamestartscene::on_online_clicked()
{
    // 创建网络管理器
    NetworkManager* networkManager = new NetworkManager();
    
    // 创建房间界面
    LobbyRoom* lobby = new LobbyRoom();
    lobby->SetNetworkManager(networkManager);
    
    // 连接游戏开始信号
    connect(lobby, &LobbyRoom::gameStarting, this, [=]() {
        // 创建游戏实例
        Game* game = new Game();
        game->SetNetworkMode(true);
        game->SetNetworkManager(networkManager);
        
        // 设置网络游戏
        bool useAI = (networkManager->GetPlayerCount() == 2);
        game->SetupNetworkGame(networkManager->GetLocalPlayerId(), useAI);
        
        // 连接网络事件到游戏
        connect(networkManager, &NetworkManager::callLandlordReceived, 
                [=](int playerId, int score) {
            game->OnNetworkCallLandlord(playerId, score);
        });
        connect(networkManager, &NetworkManager::discardCardsReceived, 
                [=](int playerId, const std::vector<int>& cards) {
            game->OnNetworkDiscardCards(playerId, cards);
        });
        connect(networkManager, &NetworkManager::passReceived, 
                [=](int playerId) {
            game->OnNetworkPass(playerId);
        });
        
        // 处理AI操作接收（非房主）
        connect(networkManager, &NetworkManager::aiActionReceived,
                [=](int aiId, MessageType type, const QJsonObject& data) {
            if (type == MSG_DISCARD_CARDS) {
                QJsonArray cards = data["cards"].toArray();
                std::vector<int> cardList;
                for (const QJsonValue& v : cards) {
                    cardList.push_back(v.toInt());
                }
                game->OnNetworkDiscardCards(aiId, cardList);
            } else if (type == MSG_PASS) {
                game->OnNetworkPass(aiId);
            } else if (type == MSG_CALL_LANDLORD) {
                int score = data["score"].toInt();
                game->OnNetworkCallLandlord(aiId, score);
            }
        });
        
        // 启动游戏
        game->GameStart();
        
        // 提示：需要修改IngameScene以支持外部Game实例
        // 目前IngameScene使用内部m_game成员，需要重构才能完全集成
        // 这里先显示一个提示
        QMessageBox::information(nullptr, "联机游戏已开始", 
            QString("联机游戏已启动！\n玩家ID: %1\n使用AI: %2\n\n注意：完整集成需要修改IngameScene以使用外部Game实例。")
            .arg(networkManager->GetLocalPlayerId())
            .arg(useAI ? "是" : "否"));
        
        // TODO: 创建游戏界面（需要重构IngameScene）
        // IngameScene* gameScene = new IngameScene();
        // gameScene->show();
        
        // 暂时不关闭，让用户可以继续操作
        // lobby->close();
        // this->close();
    });
    
    // 处理返回主菜单
    connect(lobby, &LobbyRoom::backToMenu, this, [=]() {
        lobby->close();
        this->show();
    });
    
    lobby->show();
    this->hide();
}

//退出程序
void Gamestartscene::on_exit_clicked()
{
    this -> close();
}

